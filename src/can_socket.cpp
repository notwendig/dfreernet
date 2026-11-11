#include "can_socket.hpp"
#include "can_writer.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
namespace rnet {
namespace {
class InterfaceLease {
  int fd_ = -1;

public:
  explicit InterfaceLease(const std::string &interface) {
    auto name = "/tmp/rnet-socketcan-" + std::to_string(::geteuid()) + "-" +
                interface + ".lock";
    fd_ = ::open(name.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
    if (fd_ < 0)
      throw std::runtime_error("Cannot open CAN client lock");
    struct stat info{};
    if (::fstat(fd_, &info) || info.st_uid != ::geteuid() ||
        ::flock(fd_, LOCK_EX | LOCK_NB)) {
      ::close(fd_);
      fd_ = -1;
      throw std::runtime_error(
          "Another R-net client owns the CAN interface; no requests sent");
    }
  }
  ~InterfaceLease() {
    if (fd_ >= 0)
      ::close(fd_);
  }
};
class SocketCanReadTransport : public CanTransport {
  int fd_ = -1;
  std::unique_ptr<InterfaceLease> lease_;
  std::ofstream log_;
  bool pid68_ = false;
  RepositoryFile permitted_;
  std::uint16_t payloadCrc_ = 0, identityCrc_ = 0;
  bool permitted(const CanFrame &frame) const {
    if (!pid68_)
      return allowed(frame);
    if ((frame.id & 0xffff0000) == 0x1e3c0000 ||
        (frame.id & 0xffff0000) == 0x1e3d0000) {
      unsigned sequence = frame.id & 65535;
      if (!sequence)
        return false;
      auto offset = std::size_t(sequence - 1) * 8;
      if (offset >= permitted_.data.size())
        return false;
      auto size = std::min<std::size_t>(8, permitted_.data.size() - offset);
      bool last = offset + size == permitted_.data.size();
      return frame.id == ((last ? 0x1e3d0000u : 0x1e3c0000u) | sequence) &&
             frame.data == Bytes(permitted_.data.begin() + offset,
                                 permitted_.data.begin() + offset + size);
    }
    if (frame.id == 0x78f && frame.data.size() == 8) {
      if ((frame.data[0] & 15) != 2 || frame.data[2] || frame.data[3])
        return false;
      unsigned value = 0;
      for (unsigned i = 0; i < 4; ++i)
        value |= unsigned(frame.data[4 + i]) << (8 * i);
      unsigned tc = frame.data[0] & 0xf0, odi = frame.data[1];
      if (tc == 0x20 && odi == 0x81 && value != permitted_.id)
        return false;
      if (allowed(frame))
        return true;
      if (tc == 0x20)
        return (odi == 0x82 && value == permitted_.data.size()) || (odi == 0x84 && value == 1) ||
               (odi == 0x89 && value == permitted_.version) ||
               (odi == 0x88 && value == permitted_.id) ||
               (odi == 0x8a && value == 0) ||
               (odi == 0x8f && value == identityCrc_);
      if (tc == 0x10)
        return odi == 0x8c && value == permitted_.data.size();
      if (tc == 0x80)
        return odi == 0x8c && value == payloadCrc_;
      return false;
    }
    return allowed(frame);
  }
  void record(const char *direction, const CanFrame &frame) {
    log_ << direction << ' ' << std::hex << std::uppercase << frame.id << '#';
    for (auto byte : frame.data)
      log_ << std::setw(2) << std::setfill('0') << unsigned(byte);
    log_ << '\n';
    log_.flush();
  }
  static bool allowed(const CanFrame &frame) {
    if ((frame.id & 0xffff0000) == 0x1e3f0000)
      return frame.data.empty() && (frame.id & 65535) != 0;
    if (frame.id != 0x78f || frame.data.size() != 8 ||
        (frame.data[0] & 15) >= 15 || frame.data[2] || frame.data[3])
      return false;
    unsigned value = 0;
    for (unsigned i = 0; i < 4; ++i)
      value |= unsigned(frame.data[4 + i]) << (8 * i);
    unsigned tc = frame.data[0] & 0xf0, odi = frame.data[1];
    if (tc == 0x20)
      return (odi == 0x80 && (value == 0 || value == 0x1f)) || odi == 0x81 ||
             ((odi == 0x84 || odi == 0x85) && value == 0);
    if (tc == 0x40)
      return (odi == 0x80 || odi == 0x86 || odi == 0x89 || odi == 0x8a ||
              odi == 0x8b) &&
             value == 0;
    if (tc == 0x50)
      return odi == 0x8c && (value >> 24) == 64 &&
             (value & 0xffffff) <= 65535 * 8;
    return tc == 0x80 && odi == 0x8c && value == 0;
  }

public:
  SocketCanReadTransport(const std::string &interface,
                         const std::filesystem::path &log)
      : log_(log) {
    if (interface.empty() || interface.size() >= IFNAMSIZ || !log_)
      throw std::runtime_error("Invalid read transport interface/log");
    if (interface.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOP"
                                    "QRSTUVWXYZ0123456789_.-") !=
        std::string::npos)
      throw std::runtime_error("Invalid CAN interface name");
    lease_ = std::make_unique<InterfaceLease>(interface);
    fd_ = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW);
    if (fd_ < 0)
      throw std::runtime_error("Cannot open SocketCAN read transport");
    ifreq request{};
    std::strcpy(request.ifr_name, interface.c_str());
    if (ioctl(fd_, SIOCGIFINDEX, &request) < 0) {
      close(fd_);
      fd_ = -1;
      throw std::runtime_error("Unknown CAN interface");
    }
    sockaddr_can address{};
    address.can_family = AF_CAN;
    address.can_ifindex = request.ifr_ifindex;
    if (bind(fd_, reinterpret_cast<sockaddr *>(&address), sizeof(address))) {
      close(fd_);
      fd_ = -1;
      throw std::runtime_error("Cannot bind CAN interface");
    }
    int buffer = 1024 * 1024;
    setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &buffer, sizeof(buffer));
  }
  void permitPid68(const RepositoryFile &before, const RepositoryFile &after) {
    validate_repository_change(before, after);
    permitted_ = after;
    pid68_ = true;
    payloadCrc_ = RnetDocument::crc16(after.data.data(), after.data.size());
    Bytes checked;
    for (auto n : {after.id, after.version, std::uint32_t(after.data.size())})
      for (unsigned i = 0; i < 4; ++i)
        checked.push_back(std::uint8_t(n >> (8 * i)));
    checked.insert(checked.end(), after.data.begin(), after.data.end());
    identityCrc_ = RnetDocument::crc16(checked.data(), checked.size());
  }
  ~SocketCanReadTransport() override {
    if (fd_ >= 0)
      close(fd_);
  }
  void send(const CanFrame &frame) override {
    if (!permitted(frame))
      throw std::runtime_error(
          "CAN frame outside the authorized repository policy");
    can_frame packet{};
    packet.can_id = frame.id | (frame.id > CAN_SFF_MASK ? CAN_EFF_FLAG : 0);
    packet.can_dlc = frame.data.size();
    std::copy(frame.data.begin(), frame.data.end(), packet.data);
    auto end =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    for (;;) {
      auto written = write(fd_, &packet, sizeof(packet));
      if (written == sizeof(packet))
        break;
      if (!pid68_ || written >= 0 ||
          (errno != ENOBUFS && errno != EAGAIN && errno != EWOULDBLOCK &&
           errno != EINTR) ||
          std::chrono::steady_clock::now() >= end)
        throw std::runtime_error("CAN request send failed");
      // Retry only frames that the kernel explicitly did not enqueue.
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    record("TX", frame);
  }
  bool receive(CanFrame &frame, unsigned timeout_ms) override {
    pollfd wait{fd_, POLLIN, 0};
    int ready = poll(&wait, 1, int(timeout_ms));
    if (ready < 0 || wait.revents & (POLLERR | POLLHUP | POLLNVAL))
      throw std::runtime_error("CAN read transport failed");
    if (!ready)
      return false;
    can_frame packet{};
    if (read(fd_, &packet, sizeof(packet)) != sizeof(packet))
      return false;
    if (packet.can_id & (CAN_ERR_FLAG | CAN_RTR_FLAG))
      return false;
    if (packet.can_dlc > 8)
      throw std::runtime_error("Invalid CAN frame length");
    frame.id = packet.can_id &
               ((packet.can_id & CAN_EFF_FLAG) ? CAN_EFF_MASK : CAN_SFF_MASK);
    frame.data.assign(packet.data, packet.data + packet.can_dlc);
    record("RX", frame);
    return true;
  }
};
} // namespace
std::unique_ptr<CanTransport> open_can_pid68_transport(
    const std::string &interface, const std::filesystem::path &log,
    const RepositoryFile &before, const RepositoryFile &after) {
  validate_pid68_change(before, after);
  auto transport = std::make_unique<SocketCanReadTransport>(interface, log);
  transport->permitPid68(before, after);
  return transport;
}
std::unique_ptr<CanTransport> open_can_write_transport(
    const std::string &interface, const std::filesystem::path &log,
    const RepositoryFile &before, const RepositoryFile &after) {
  validate_repository_change(before,after);
  auto transport=std::make_unique<SocketCanReadTransport>(interface,log);
  transport->permitPid68(before,after);
  return transport;
}
std::unique_ptr<CanTransport>
open_can_read_transport(const std::string &interface,
                        const std::filesystem::path &log) {
  return std::make_unique<SocketCanReadTransport>(interface, log);
}
} // namespace rnet
