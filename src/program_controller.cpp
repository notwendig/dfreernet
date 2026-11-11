#include "can_socket.hpp"
#include "can_writer.hpp"
#include "database.hpp"
#include "runtime_resources.hpp"
#include "values.hpp"
#include <algorithm>
#include <csignal>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <unistd.h>
namespace {
std::atomic<bool> cancelled{false};
void cancel(int) { cancelled = true; }
void save(const std::filesystem::path &path, const rnet::Bytes &bytes) {
  int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
  if (fd < 0)
    throw std::runtime_error("Refuse existing/inaccessible backup output");
  std::size_t offset = 0;
  bool ok = true;
  while (offset < bytes.size()) {
    auto n = ::write(fd, bytes.data() + offset, bytes.size() - offset);
    if (n <= 0) {
      ok = false;
      break;
    }
    offset += n;
  }
  if (::fsync(fd))
    ok = false;
  if (::close(fd))
    ok = false;
  if (!ok)
    throw std::runtime_error(
        "Durable backup write failed; no programming allowed");
}
} // namespace
int main(int argc, char **argv) {
  try {
    // Legacy callers may still pass an ignored installation argument.
    if(argc==8){for(int i=2;i<argc-1;++i)argv[i]=argv[i+1];--argc;}
    if (argc!=7 || std::string(argv[5])!="--stationary-confirmed" ||
        std::string(argv[6]).size()!=16 || std::string(argv[6]).substr(0,15)!="--access-level=" ||
        argv[6][15]<'0' || argv[6][15]>'3')
      throw std::runtime_error("Usage: rnet_program_controller INTERFACE BEFORE DESIRED OUTPUT --stationary-confirmed --access-level=0..3");
    std::signal(SIGINT,cancel);std::signal(SIGTERM,cancel);
    std::cout << std::unitbuf;
    auto level=rnet::AccessLevel(argv[6][15]-'0');
    std::filesystem::path output=argv[4];
    auto catalog=rnet::bundled_catalog();
    auto beforeBytes=rnet::read_file(argv[2]), desiredBytes=rnet::read_file(argv[3]);
    auto before=rnet::RnetDocument::read(beforeBytes), desired=rnet::RnetDocument::read(desiredBytes);
    auto files=rnet::controller_write_plan(before,desired,catalog,level);
    if (files.empty()) throw std::runtime_error("No parameter changes to write");
    if (std::filesystem::exists(output)) throw std::runtime_error("Choose a new programming output directory");
    std::filesystem::create_directories(output);
    save(output/"before.R-net",beforeBytes);save(output/"desired.R-net",desiredBytes);
    // Check every affected file before committing the first one. Each writer
    // repeats the exact comparison under held ownership before its own commit.
    {
      auto transport=rnet::open_can_read_transport(argv[1],output/"preflight-can.log");
      rnet::CanRepositoryReader reader(*transport,cancelled,2);
      if (reader.repository_status()!=0) throw std::runtime_error("Repository already owned");
      for (auto i:files) {
        auto current=reader.read(before.files[i].id);
        if (current.version!=before.files[i].version || current.data!=before.files[i].data)
          throw std::runtime_error("Stale configuration; read controller before writing");
        save(output/(std::to_string(current.id)+"-fresh-before.bin"),current.data);
      }
    }
    unsigned completed=0;
    for (auto i:files) {
      if (cancelled) throw std::runtime_error("Cancelled before next file; inspect completed files and read controller before retry");
      auto key=std::to_string(before.files[i].id);
      auto transport=rnet::open_can_write_transport(argv[1],output/(key+"-can.log"),before.files[i],desired.files[i]);
      rnet::CanRepositoryWriter writer(*transport,cancelled,2);
      auto result=writer.program(before.files[i],desired.files[i],[&](const auto &fresh,const auto &prepared) {
        save(output/(key+"-before.bin"),fresh.data);
        save(output/(key+"-prepared.bin"),prepared.data);
        int directory=::open(output.c_str(),O_RDONLY|O_DIRECTORY);
        if (directory<0) throw std::runtime_error("Cannot sync backup directory");
        auto status=::fsync(directory);::close(directory);
        if (status) throw std::runtime_error("Cannot persist backup directory");
        std::cout << "BACKUP_DURABLE file=" << key << "\n";
      });
      save(output/(key+"-readback.bin"),result.readback.data);
      desired.files[i].data=result.readback.data;
      std::cout << "READBACK_VERIFIED file=" << key << " commit_ack=" << result.commit_acknowledged << "\n";
      ++completed;
      std::cout << "VERIFIED_FILES " << completed << "/" << files.size() << "\n";
    }
    save(output/"verified.R-net",desired.write());
    std::cout << "RELEASE_VERIFIED node=2 owner_status=0\nCOMPLETE write verified_files=" << completed << "\n";
    return 0;
  } catch(const rnet::ControllerReadbackError &error) {
    std::cerr << (error.commit_acknowledged?"COMMIT_ACKNOWLEDGED_READBACK_PENDING":"COMMIT_SENT_READBACK_PENDING")
              << "\nFAILED: " << error.what() << "; do not repeat writing. Read controller to verify the actual result.\n";
    return 2;
  } catch(const std::exception &error) {
    std::cerr << "FAILED: " << error.what() << "; no automatic write retry. Some files may already be committed; read controller and inspect backup before retry.\n";
    return 1;
  }
}
