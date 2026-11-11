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
    if (argc != 8 || std::string(argv[5]) != "--stationary-confirmed" ||
        std::string(argv[6]) != "--pid68=1" ||
        std::string(argv[7]).size() != 16 ||
        std::string(argv[7]).substr(0, 15) != "--access-level=" ||
        argv[7][15] < '0' || argv[7][15] > '3')
      throw std::runtime_error(
          "Usage: rnet_program_pid68 INTERFACE INSTALLATION FRESH_FULL_BACKUP "
          "OUTPUT_DIR --stationary-confirmed --pid68=1 --access-level=0..3");
    const auto access = rnet::AccessLevel(argv[7][15] - '0');
    std::signal(SIGINT, cancel);
    std::signal(SIGTERM, cancel);
    std::filesystem::path output = argv[4];
    if (std::filesystem::exists(output))
      throw std::runtime_error("Choose a new programming output directory");
    std::filesystem::create_directories(output);
    auto catalog = rnet::bundled_catalog();
    const auto &definition = catalog.parameters.at(68);
    if (definition.name() != u"OBP Keycode Entry" ||
        definition.extension[0] != 1)
      throw std::runtime_error("Unexpected PID68 definition");
    const auto source = rnet::read_file(argv[3]);
    auto before = rnet::RnetDocument::read(source);
    auto prepared = before;
    std::vector<std::string> warnings;
    auto values = rnet::read_values(before, catalog, &warnings);
    const rnet::DocumentValue *selected = nullptr;
    for (const auto &value : values)
      if (value.parameter == 68) {
        if (selected)
          throw std::runtime_error("Ambiguous PID68");
        selected = &value;
      }
    if (!selected || selected->group || selected->profile ||
        selected->width != 1 || selected->offset != 16)
      throw std::runtime_error("PID68 storage not verified");
    auto index = selected->file;
    const auto &expected = before.files.at(index);
    rnet::edit_value(prepared, catalog, *selected, u"1",
                     access);
    auto next = prepared.files[index];
    rnet::validate_pid68_change(expected, next);
    auto afterValues = rnet::read_values(prepared, catalog, &warnings);
    if (afterValues.size() != values.size())
      throw std::runtime_error("Changed mapped value count");
    unsigned changed = 0;
    for (std::size_t i = 0; i < values.size(); ++i)
      if (values[i].number != afterValues[i].number ||
          values[i].text != afterValues[i].text) {
        if (values[i].parameter != 68 || afterValues[i].number != 1)
          throw std::runtime_error("Unrequested parameter change");
        ++changed;
      }
    if (changed > 1)
      throw std::runtime_error("More than one parameter would change");
    auto transport = rnet::open_can_pid68_transport(argv[1], output / "can.log",
                                                    expected, next);
    rnet::CanPid68Writer writer(*transport, cancelled);
    auto result = writer.program(
        expected, next, [&](const auto &fresh, const auto &desired) {
          save(output / "before.R-net", source);
          save(output / "prepared-PID68-1.R-net", prepared.write());
          save(output / "before-10006-v22.bin", fresh.data);
          save(output / "prepared-10006-v22.bin", desired.data);
          int directory = ::open(output.c_str(), O_RDONLY | O_DIRECTORY);
          if (directory < 0)
            throw std::runtime_error("Cannot sync backup directory");
          auto status = ::fsync(directory);
          ::close(directory);
          if (status)
            throw std::runtime_error("Cannot persist backup directory");
          std::cout << "BACKUP_DURABLE PID=68 old=" << selected->number
                    << " new=1 file=00010006 version=22 bytes=623\n"
                    << std::flush;
        });
    save(output / "readback-10006-v22.bin", result.readback.data);
    prepared.files[index].data = result.readback.data;
    save(output / "verified-PID68-1.R-net", prepared.write());
    std::cout
        << "READBACK_VERIFIED PID=68 value=1 complete_file_match=1 commit_ack="
        << result.commit_acknowledged << " changed=" << result.changed
        << "\nRELEASE_VERIFIED node=2 owner_status=0\nCOMPLETE PID68=1\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr
        << "FAILED: " << error.what()
        << "; inspect backups and read controller state before any retry\n";
    return 1;
  }
}
