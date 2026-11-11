#include "can_writer.hpp"
#include <algorithm>
#include <chrono>
#include <thread>
namespace rnet {
namespace {
std::uint32_t value32(const Bytes &b) {
  if (b.size() != 8)
    throw std::runtime_error("Short download response");
  std::uint32_t n = 0;
  for (unsigned i = 0; i < 4; ++i)
    n |= std::uint32_t(b[i + 4]) << (8 * i);
  return n;
}
Bytes identity(const RepositoryFile &file) {
  Bytes result;
  for (auto n : {file.id, file.version, std::uint32_t(file.data.size())})
    for (unsigned i = 0; i < 4; ++i)
      result.push_back(std::uint8_t(n >> (8 * i)));
  result.insert(result.end(), file.data.begin(), file.data.end());
  return result;
}
bool same(const RepositoryFile &a, const RepositoryFile &b) {
  return a.id == b.id && a.version == b.version && a.data == b.data;
}
} // namespace
void validate_pid68_change(const RepositoryFile &before,
                           const RepositoryFile &after) {
  if (before.id != 0x10006 || before.version != 22 ||
      before.data.size() != 623 || after.id != before.id ||
      after.version != before.version ||
      after.data.size() != before.data.size())
    throw std::runtime_error("Unsupported PID68 file identity/version/size");
  auto layout = RepositoryLayout::read(before.data);
  RepositoryLayout::read(after.data);
  if (layout.header_size != 10 || layout.stride[0] != 293 ||
      layout.stride[1] != 40 || before.data[16] > 1 || after.data[16] != 1)
    throw std::runtime_error("Unexpected PID68 storage layout/value");
  std::size_t groupStart = layout.header_size;
  for (unsigned group = 0; group < 2; ++group) {
    for (unsigned profile = 0; profile < (group ? 8u : 1u); ++profile) {
      auto start = groupStart + profile * layout.stride[group];
      std::uint32_t sum = 0;
      for (auto i = start + 2; i < start + layout.stride[group]; ++i)
        sum += before.data[i];
      if (std::uint16_t(~sum) != (unsigned(before.data[start]) |
                                  (unsigned(before.data[start + 1]) << 8)))
        throw std::runtime_error("Invalid pre-write group checksum");
    }
    groupStart += layout.stride[group] * (group ? 8u : 1u);
  }
  auto expected = before.data;
  expected[16] = 1;
  std::uint32_t sum = 0;
  for (std::size_t i = 12; i < 303; ++i)
    sum += expected[i];
  auto checksum = std::uint16_t(~sum);
  expected[10] = std::uint8_t(checksum);
  expected[11] = std::uint8_t(checksum >> 8);
  if (after.data != expected)
    throw std::runtime_error(
        "PID68 plan modifies bytes other than PID68 and its group checksum");
}
void validate_repository_change(const RepositoryFile &before, const RepositoryFile &after) {
  if (before.id != after.id || before.version != after.version ||
      before.data.size() != after.data.size() || before.data.size() < 4 ||
      before.data.size() > 65535*8)
    throw std::runtime_error("Repository identity/version/size changed");
  const auto layout = RepositoryLayout::read(before.data);
  RepositoryLayout::read(after.data);
  if (!std::equal(before.data.begin(), before.data.begin()+layout.header_size,
                  after.data.begin()))
    throw std::runtime_error("Repository layout changed");
  // Both old and new files must have valid checksums in every group.
  for (const auto *file : {&before, &after}) {
    std::size_t start = layout.header_size;
    for (unsigned group=0; group<2; ++group) {
      if (!layout.stride[group]) continue;
      auto count=group ? 8u : 1u;
      for (unsigned profile=0; profile<count; ++profile) {
        auto offset=start+profile*layout.stride[group];
        if (layout.stride[group]<2) throw std::runtime_error("Invalid group stride");
        std::uint32_t sum=0;
        for (auto i=offset+2; i<offset+layout.stride[group]; ++i) sum+=file->data.at(i);
        if (std::uint16_t(~sum)!=(unsigned(file->data.at(offset)) | (unsigned(file->data.at(offset+1))<<8)))
          throw std::runtime_error("Invalid repository group checksum");
      }
      start+=count*layout.stride[group];
    }
  }
}
void CanRepositoryWriter::release() noexcept {
  // The original WriteFileData always disables SlotWrite, even on failure.
  // Never finish an incomplete download with a made-up CRC or commit it.
  for (auto odi : {0x84, 0x80}) {
    try {
      transport_.send({0x78f, Bytes{std::uint8_t(0x20 | node_),
                                    std::uint8_t(odi), 0, 0, 0, 0, 0, 0}});
      auto end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
      while (std::chrono::steady_clock::now() < end) {
        CanFrame f;
        if (transport_.receive(f, 50) && f.id == 0x790 + node_ &&
            f.data.size() == 8 && f.data[0] == 0x4f && f.data[1] == odi)
          break;
      }
    } catch (...) {
    }
  }
}
Pid68ProgramResult CanRepositoryWriter::program(
    const RepositoryFile &expectedBefore, const RepositoryFile &after,
    const std::function<void(const RepositoryFile &, const RepositoryFile &)>
        &backup) {
  validate_repository_change(expectedBefore, after);
  if (!backup)
    throw std::runtime_error("Durable backup callback required");
  CanRepositoryReader reader(transport_, cancelled_, node_);
  if (reader.repository_status() != 0)
    throw std::runtime_error("Repository is already owned; no write attempted");
  bool owned = false, commitSent = false, commitAck = false,
       changed = expectedBefore.data != after.data;
  try {
    // Fully re-read while retaining ownership. A stale plan never reaches a
    // write.
    auto current = reader.read_impl(expectedBefore.id, true);
    owned = true;
    if (!same(current, expectedBefore))
      throw std::runtime_error(
          "Controller file changed since backup; no write attempted");
    auto attributes = reader.quick(0x40, 0x8a);
    if (attributes != 0)
      throw std::runtime_error(
          "Unsupported PID68 file attributes; no write attempted");
    backup(current, after);
    if (changed) {
      // Original CRnetInterface::WriteFileData: allocate a BUFFER slot.
      // Never choose/overwrite another real file as an invented staging slot.
      reader.quick(0x20, 0x82, after.data.size());
      reader.quick(0x20, 0x84, 1);
      reader.quick(0x20, 0x85, 0);
      reader.send(0x10, 0x8c, after.data.size());
      auto ready = reader.wait([&](const auto &f) {
        return repository_reply(f,node_,0x8c,{0x4f,0xcf});
      });
      if (ready.data[0] == 0xcf)
        throw PopRejected(0x8c, value32(ready.data));
      if (ready.data[0] != 0x4f)
        throw std::runtime_error("Unexpected segmented download response");
      auto offered = value32(ready.data);
      auto size = offered & 0xffffff;
      unsigned window = offered >> 24;
      if (!window)
        window = 256; // original DLL handler
      if (size && size != after.data.size())
        throw std::runtime_error("Download peer size differs");
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(20);
      unsigned sequence = 1;
      std::size_t offset = 0;
      while (offset < after.data.size()) {
        unsigned sent = 0, lastSequence = 0;
        while (sent < window && offset < after.data.size()) {
          if (std::chrono::steady_clock::now() > deadline)
            throw std::runtime_error(
                "Download deadline exceeded before commit");
          if (cancelled_)
            throw std::runtime_error(
                "PID68 programming cancelled before commit");
          auto length = std::min<std::size_t>(8, after.data.size() - offset);
          bool last = offset + length == after.data.size();
          transport_.send({0x1e3c0000u | (last ? 0x10000u : 0u) | sequence,
                           Bytes(after.data.begin() + offset,
                                 after.data.begin() + offset + length)});
          offset += length;
          lastSequence = sequence++;
          ++sent;
          std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        auto ack = reader.wait([&](const auto &f) {
          return (f.id & 0xffff0000) == (((0x790 + node_) << 18) | 0x10000) ||
                 (f.id == 0x790 + node_ && f.data.size() == 8 &&
                  f.data[0] == 0xcf && f.data[1] == 0x8c);
        });
        if (ack.id == 0x790 + node_)
          throw PopRejected(0x8c, value32(ack.data));
        if (!ack.data.empty() ||
            ((ack.id & 65535) != 0 && (ack.id & 65535) != lastSequence))
          throw std::runtime_error("Invalid download segment acknowledgment");
      }
      auto crc = RnetDocument::crc16(after.data.data(), after.data.size());
      reader.send(0x80, 0x8c, crc);
      auto end = reader.wait([&](const auto &f) {
        return repository_reply(f,node_,0x8c,{0x8f,0xcf});
      });
      if (end.data[0] != 0x8f)
        throw std::runtime_error("Download data CRC not acknowledged");
      reader.quick(0x20, 0x89, after.version);
      reader.quick(0x20, 0x88, after.id);
      reader.quick(0x20, 0x8a, attributes);
      auto checked = identity(after);
      auto expectedCrc = RnetDocument::crc16(checked.data(), checked.size());
      if (reader.quick(0x40, 0x8b) != expectedCrc)
        throw std::runtime_error("Staged file CRC differs; commit refused");
      commitSent = true;
      reader.quick(0x20, 0x8f, expectedCrc);
      commitAck = true;
    }
    reader.quick(0x20, 0x84, 0);
    reader.quick(0x20, 0x80, 0);
    owned = false;
  } catch (...) {
    if (owned)
      release();
    if (!commitSent)
      throw; /* no retry: verify actual result below */
  }
  // Verification is necessary even if cancellation/ACK loss followed the
  // commit.
  std::atomic<bool> verifyCancel{false};
  CanRepositoryReader verify(transport_, verifyCancel, node_);
  // Acknowledged commit/release can be followed by nonzero states (observed
  // 0x10 and 0x12). Do not interpret an unknown state as failure or free:
  // poll status only, bounded, without reacquiring or repeating any write.
  try {
  const auto settleDeadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(30);
  for (;;) {
    std::uint32_t status;
    try{status=verify.repository_status();}
    catch(const CanResponseTimeout &){
      // Controller-internal reconfiguration may interrupt one status reply.
      // Repeat only this harmless read, never acquisition/download/commit.
      if(std::chrono::steady_clock::now()>=settleDeadline)throw;
      continue;
    }
    if (!status)
      break;
    if (std::chrono::steady_clock::now() >= settleDeadline)
      throw std::runtime_error(
          "Repository not ready for post-write readback (status " + std::to_string(status) + "); do not repeat write");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
  }
  auto actual = verify.read(expectedBefore.id);
  if (!same(actual, after))
    throw std::runtime_error("PID68 write not confirmed by complete file "
                             "readback; do not retry automatically");
  if (verify.repository_status() != 0)
    throw std::runtime_error(
        "PID68 readback matched but repository release is not confirmed");
  return {std::move(actual), changed, commitAck, true};
  } catch(const std::exception &error) {
    if(commitSent)throw ControllerReadbackError(error.what(),commitAck);
    throw;
  }
}
} // namespace rnet
