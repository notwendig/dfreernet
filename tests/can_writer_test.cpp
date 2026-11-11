#include "can_writer.hpp"
#include <algorithm>
#include <deque>
#include <iostream>
#include <thread>
using namespace rnet;
namespace {
void check(bool ok, const char *text) {
  if (!ok)
    throw std::runtime_error(text);
}
std::uint32_t u32(const Bytes &b) {
  std::uint32_t v = 0;
  for (unsigned i = 0; i < 4; ++i)
    v |= unsigned(b.at(i + 4)) << (8 * i);
  return v;
}
std::uint16_t fileCrc(const RepositoryFile &f) {
  Bytes b;
  for (auto v : {f.id, f.version, unsigned(f.data.size())})
    for (unsigned i = 0; i < 4; ++i)
      b.push_back(std::uint8_t(v >> (8 * i)));
  b.insert(b.end(), f.data.begin(), f.data.end());
  return RnetDocument::crc16(b.data(), b.size());
}
void fixCommon(Bytes &b) {
  std::uint32_t sum = 0;
  for (unsigned i = 12; i < 303; ++i)
    sum += b[i];
  auto crc = std::uint16_t(~sum);
  b[10] = crc;
  b[11] = crc >> 8;
}
struct Peer : CanTransport {
  enum Mode {
    Normal,
    Busy,
    RejectBuffer,
    BadReady,
    CancelMidway,
    BadStagedCrc,
    LostCommitAck,
    BusyAfterCommit,
    Busy18AfterCommit,
    BadReadback,
    NonzeroAttributes
  };
  Mode mode;
  RepositoryFile file, before, desired;
  std::atomic<bool> &cancel;
  std::deque<CanFrame> queue;
  unsigned owner = 0, readOffset = 0, readSeq = 1, writeSeq = 1, commits = 0,
           allocations = 0, settlePolls = 0;
  bool writing = false, allocated = false, versionSet = false, typeSet = false,
       attributesSet = false;
  Bytes staged;
  std::vector<CanFrame> tx;
  Peer(const RepositoryFile &a, const RepositoryFile &b, std::atomic<bool> &c,
       Mode m)
      : mode(m), file(a), before(a), desired(b), cancel(c) {
    if (m == Busy)
      owner = 7;
  }
  void reply(unsigned tc, unsigned odi, unsigned value = 0) {
    Bytes data{std::uint8_t(tc), std::uint8_t(odi), 0, 0};
    for (unsigned i = 0; i < 4; ++i)
      data.push_back(value >> (8 * i));
    queue.push_back({0x792, data});
  }
  void readWindow() {
    for (unsigned count = 0; count < 64 && readOffset < file.data.size();
         ++count) {
      auto n = std::min<std::size_t>(8, file.data.size() - readOffset);
      bool last = readOffset + n == file.data.size();
      queue.push_back({(0x792u << 18) | (last ? 0x30000u : 0x20000u) | readSeq,
                       Bytes(file.data.begin() + readOffset,
                             file.data.begin() + readOffset + n)});
      readOffset += n;
      ++readSeq;
    }
  }
  void send(const CanFrame &f) override {
    tx.push_back(f);
    if ((f.id & 0xffff0000) == 0x1e3f0000) {
      check((f.id & 65535) == readSeq - 1, "bad read ACK");
      if (readOffset < file.data.size())
        readWindow();
      else
        reply(0x8f, 0x8c,
              RnetDocument::crc16(file.data.data(), file.data.size()));
      return;
    }
    if ((f.id & 0xffff0000) == 0x1e3c0000 ||
        (f.id & 0xffff0000) == 0x1e3d0000) {
      check(writing && allocated, "write segment without buffer");
      check((f.id & 65535) == writeSeq, "bad write sequence");
      auto offset = std::size_t(writeSeq - 1) * 8;
      auto n = std::min<std::size_t>(8, desired.data.size() - offset);
      check(f.data == Bytes(desired.data.begin() + offset,
                            desired.data.begin() + offset + n),
            "changed unrequested payload");
      bool last = offset + n == desired.data.size();
      check(bool(f.id & 0x10000) == last, "bad write last marker");
      staged.insert(staged.end(), f.data.begin(), f.data.end());
      if (mode == CancelMidway && writeSeq == 4)
        cancel = true;
      if (last || writeSeq % 64 == 0)
        queue.push_back({0x1e490000, {}});
      ++writeSeq;
      return;
    }
    check(f.id == 0x78f && f.data.size() == 8 && (f.data[0] & 15) == 2,
          "unexpected non-POP request");
    unsigned tc = f.data[0] & 0xf0, odi = f.data[1], v = u32(f.data);
    if (tc == 0x40 && odi == 0x80) {
      if ((mode == BusyAfterCommit || mode == Busy18AfterCommit) && commits && owner == 0 && settlePolls++ < 3) {
        reply(0x2f, odi, mode == Busy18AfterCommit && settlePolls < 3 ? 18 : 16);
        return;
      }
      reply(0x2f, odi, owner);
      return;
    }
    if (tc == 0x20 && odi == 0x80) {
      check(v == 0 || v == 0x1f, "bad owner");
      if (v == 0)
        check(!writing, "release with SlotWrite enabled");
      owner = v;
      reply(0x4f, odi);
      return;
    }
    check(owner == 0x1f, "transaction without ownership");
    if (tc == 0x20 && odi == 0x81) {
      check(v == 0x10006, "unrequested real file selected");
      reply(0x4f, odi);
      return;
    }
    if (tc == 0x20 && odi == 0x84) {
      check(v <= 1, "bad SlotWrite");
      writing = v;
      if (!v)
        allocated = false;
      reply(0x4f, odi);
      return;
    }
    if (tc == 0x20 && odi == 0x85) {
      check(v == 0, "bad location");
      reply(0x4f, odi);
      return;
    }
    if (tc == 0x40 && odi == 0x89) {
      reply(0x2f, odi, file.version);
      return;
    }
    if (tc == 0x40 && odi == 0x86) {
      reply(0x2f, odi, file.data.size());
      return;
    }
    if (tc == 0x40 && odi == 0x8a) {
      reply(0x2f, odi, mode == NonzeroAttributes ? 1 : 0);
      return;
    }
    if (tc == 0x40 && odi == 0x8b) {
      auto crc = allocated && typeSet ? fileCrc(desired) : fileCrc(file);
      if (mode == BadStagedCrc && allocated && typeSet)
        crc ^= 1;
      reply(0x2f, odi, crc);
      return;
    }
    if (tc == 0x50 && odi == 0x8c) {
      check(!writing && v == (623u | 0x40000000u), "bad read start");
      readOffset = 0;
      readSeq = 1;
      reply(0xf, odi, 623);
      readWindow();
      return;
    }
    if (tc == 0x80 && odi == 0x8c) {
      if (writing) {
        check(staged == desired.data, "incomplete download end");
        check(v == RnetDocument::crc16(staged.data(), staged.size()),
              "bad sent data CRC");
        reply(0x8f, odi);
      } else
        check(v == 0, "bad read finish");
      return;
    }
    if (tc == 0x20 && odi == 0x82) {
      check(v == 623 && !writing, "bad buffer allocation");
      ++allocations;
      if (mode == RejectBuffer) {
        reply(0xcf, odi, 0x20);
        return;
      }
      allocated = true;
      versionSet = typeSet = attributesSet = false;
      reply(0x4f, odi);
      return;
    }
    if (tc == 0x10 && odi == 0x8c) {
      check(allocated && writing && v == 623, "bad download request");
      staged.clear();
      writeSeq = 1;
      reply(0x4f, odi, (mode == BadReady ? 624 : 623) | 0x40000000u);
      return;
    }
    if (tc == 0x20 && odi == 0x89) {
      check(staged == desired.data && v == 22, "bad version");
      versionSet = true;
      reply(0x4f, odi);
      return;
    }
    if (tc == 0x20 && odi == 0x88) {
      check(versionSet && v == 0x10006, "bad source key");
      typeSet = true;
      reply(0x4f, odi);
      return;
    }
    if (tc == 0x20 && odi == 0x8a) {
      check(typeSet && v == 0, "changed attributes");
      attributesSet = true;
      reply(0x4f, odi);
      return;
    }
    if (tc == 0x20 && odi == 0x8f) {
      check(attributesSet && v == fileCrc(desired), "bad commit");
      ++commits;
      file = mode == BadReadback ? before : desired;
      if (mode == LostCommitAck)
        throw std::runtime_error("simulated lost commit ACK");
      reply(0x4f, odi);
      return;
    }
    throw std::runtime_error(
        "Unexpected/unauthorized request in fake repository");
  }
  bool receive(CanFrame &f, unsigned) override {
    if (queue.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      return false;
    }
    f = queue.front();
    queue.pop_front();
    return true;
  }
};
} // namespace
int main(int argc, char **argv) {
  try {
    check(argc == 2, "fixture required");
    RepositoryFile before;
    before.id = 0x10006;
    before.version = 22;
    before.data = read_file(argv[1]);
    auto after = before;
    after.data[16] = 1;
    fixCommon(after.data);
    validate_pid68_change(before, after);
    for (auto mode :
         {Peer::Normal, Peer::Busy, Peer::RejectBuffer, Peer::BadReady,
          Peer::CancelMidway, Peer::BadStagedCrc, Peer::LostCommitAck,
          Peer::BadReadback, Peer::NonzeroAttributes, Peer::BusyAfterCommit, Peer::Busy18AfterCommit}) {
      std::atomic<bool> cancelled{false};
      Peer peer(before, after, cancelled, mode);
      CanPid68Writer writer(peer, cancelled);
      bool backed = false, success = false;
      try {
        auto r = writer.program(before, after, [&](const auto &, const auto &) {
          check(!peer.allocations, "download before backup");
          backed = true;
        });
        check(r.readback_verified && r.readback.data == after.data,
              "false success");
        success = true;
      } catch (const std::runtime_error &) {
      }
      bool expected = mode == Peer::Normal || mode == Peer::LostCommitAck ||
                      mode == Peer::BusyAfterCommit || mode == Peer::Busy18AfterCommit;
      check(success == expected, "unexpected scenario result");
      if (expected) {
        if (mode == Peer::BusyAfterCommit || mode == Peer::Busy18AfterCommit)
          check(peer.settlePolls >= 4, "post-commit busy status not polled");
        check(backed && peer.commits == 1 && peer.file.data == after.data,
              "write/result mismatch");
      } else if (mode != Peer::BadReadback) {
        check(!peer.commits && peer.file.data == before.data,
              "failed precommit path changed controller");
      }
      if (mode == Peer::Busy)
        check(peer.owner == 7 && !peer.allocations, "touched another owner");
      else
        check(peer.owner == 0 && !peer.writing,
              "ownership/SlotWrite left enabled");
    }
    {
      std::atomic<bool> c{false};
      Peer peer(before, after, c, Peer::Normal);
      auto bad = after;
      bad.data[17] ^= 1;
      fixCommon(bad.data);
      bool denied = false;
      try {
        CanPid68Writer(peer, c).program(before, bad, [](auto &, auto &) {});
      } catch (const std::runtime_error &) {
        denied = true;
      }
      check(denied && peer.tx.empty(), "unrequested plan transmitted");
    }
    {
      std::atomic<bool> c{false};
      Peer peer(before, after, c, Peer::Normal);
      peer.file.data[17] ^= 1;
      fixCommon(peer.file.data);
      bool denied = false;
      try {
        CanPid68Writer(peer, c).program(before, after, [](auto &, auto &) {});
      } catch (const std::runtime_error &) {
        denied = true;
      }
      check(denied && !peer.allocations && peer.owner == 0,
            "stale plan reached write");
    }
    {
      std::atomic<bool> c{false};
      Peer peer(before, after, c, Peer::Normal);
      bool denied = false;
      try {
        CanPid68Writer(peer, c).program(before, after, [](auto &, auto &) {
          throw std::runtime_error("disk full");
        });
      } catch (const std::runtime_error &) {
        denied = true;
      }
      check(denied && !peer.allocations && peer.owner == 0,
            "backup failure reached write");
    }
    {
      std::atomic<bool> c{false};
      Peer peer(after, after, c, Peer::Normal);
      auto r =
          CanPid68Writer(peer, c).program(after, after, [](auto &, auto &) {});
      check(r.readback_verified && !r.changed && !peer.commits &&
                !peer.allocations && peer.owner == 0,
            "already-set case wrote data");
    }
    std::cout
        << "PASS: bounded PID68 writer; original buffer flow; exact "
           "bytes/CRC/readback; busy/stale/backup/reject/cancel/bad "
           "CRC/readback/ACK-loss/no-op cases; no automatic write retry\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
  }
}
