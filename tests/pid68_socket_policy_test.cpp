#include "can_socket.hpp"
#include "can_writer.hpp"
#include <iostream>
namespace {
rnet::CanFrame request(unsigned tc, unsigned odi, unsigned value) {
  rnet::Bytes b{std::uint8_t(tc | 2), std::uint8_t(odi), 0, 0};
  for (unsigned i = 0; i < 4; ++i)
    b.push_back(value >> (8 * i));
  return {0x78f, b};
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc != 4)
      return 2;
    rnet::RepositoryFile before;
    before.id = 0x10006;
    before.version = 22;
    before.data = rnet::read_file(argv[2]);
    auto after = before;
    after.data[16] = 1;
    unsigned sum = 0;
    for (unsigned i = 12; i < 303; ++i)
      sum += after.data[i];
    auto crc = std::uint16_t(~sum);
    after.data[10] = crc;
    after.data[11] = crc >> 8;
    {
      auto t = rnet::open_can_read_transport(
          argv[1], std::filesystem::path(argv[3]) / "read-only-guard.log");
      bool blocked = false;
      try {
        t->send(request(0x20, 0x84, 1));
      } catch (const std::runtime_error &) {
        blocked = true;
      }
      if (!blocked)
        throw std::runtime_error("Read-only transport accepted SlotWrite=1");
    }
    auto transport = rnet::open_can_pid68_transport(
        argv[1], std::filesystem::path(argv[3]) / "pid68-guard.log", before,
        after);
    bool concurrentBlocked = false;
    try {
      auto other = rnet::open_can_read_transport(
          argv[1], std::filesystem::path(argv[3]) / "concurrent.log");
    } catch (const std::runtime_error &) {
      concurrentBlocked = true;
    }
    if (!concurrentBlocked)
      throw std::runtime_error("Concurrent own POP client was not blocked");
    rnet::Bytes header;
    for (auto v : {after.id, after.version, unsigned(after.data.size())})
      for (unsigned i = 0; i < 4; ++i)
        header.push_back(v >> (8 * i));
    header.insert(header.end(), after.data.begin(), after.data.end());
    auto commit = rnet::RnetDocument::crc16(header.data(), header.size());
    std::vector<rnet::CanFrame> rejected{{0xc, {0}},
                                         {0, {0}},
                                         request(0x20, 0x81, 0x10016),
                                         request(0x20, 0x83, 0),
                                         request(0x20, 0x88, 0x10016),
                                         request(0x20, 0x89, 23),
                                         request(0x20, 0x8a, 1),
                                         request(0x20, 0x8f, commit ^ 1),
                                         request(0x10, 0x8c, 624),
                                         request(0x20, 0x85, 16),
                                         {0x1e3c0003, rnet::Bytes(8, 0)},
                                         {0x1e3d004f, {1}}};
    for (const auto &f : rejected) {
      bool blocked = false;
      try {
        transport->send(f);
      } catch (const std::runtime_error &) {
        blocked = true;
      }
      if (!blocked)
        throw std::runtime_error(
            "PID68 transport accepted an unauthorized frame");
    }
    std::cout << "PASS: read-only path still rejects writes; PID68 transport "
                 "rejects power/delete/wrong "
                 "file/version/attributes/CRC/location/payload/segment without "
                 "any CAN TX\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
