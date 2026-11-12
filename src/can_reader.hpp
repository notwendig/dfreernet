#pragma once
#include "document.hpp"
#include <atomic>
#include <functional>
#include <initializer_list>
namespace rnet {
struct CanFrame {
  std::uint32_t id = 0;
  Bytes data;
};
// Match a reply addressed to programmer F, never a same-object request
// between controller modules or a reply belonging to another client.
bool repository_reply(const CanFrame &, unsigned node, std::uint8_t odi,
                      std::initializer_list<std::uint8_t> codes);
class CanTransport {
public:
  virtual ~CanTransport() = default;
  virtual void send(const CanFrame &) = 0;
  // A timeout returns false; transport errors throw.
  virtual bool receive(CanFrame &, unsigned timeout_ms) = 0;
};
class CanResponseTimeout : public std::runtime_error {
public:
  CanResponseTimeout() : std::runtime_error("CAN response timed out") {}
};
class PopRejected : public std::runtime_error {
public:
  unsigned odi;
  std::uint32_t code;
  PopRejected(unsigned object, std::uint32_t abort)
      : std::runtime_error("POP object " + std::to_string(object) +
                           " rejected: " + std::to_string(abort)),
        odi(object), code(abort) {}
};
// Bounded read-only implementation of the observed 78F/790 and 1E42/1E43
// exchange. The plan contains selectors only, never response bytes or sizes.
class CanRepositoryReader {
  friend class CanPid68Writer;
  friend class CanRepositoryWriter;
  CanTransport &transport_;
  const std::atomic<bool> &cancelled_;
  unsigned node_ = 0;
  RepositoryFile read_impl(std::uint32_t selector, bool retainOwnership);
  void cleanup(bool transfer) noexcept;
  void send(std::uint8_t tc, std::uint8_t odi, std::uint32_t value = 0);
  std::uint32_t quick(std::uint8_t tc, std::uint8_t odi,
                      std::uint32_t value = 0);
  CanFrame wait(const std::function<bool(const CanFrame &)> &,
                unsigned timeout_ms = 3000);

public:
  CanRepositoryReader(CanTransport &t, const std::atomic<bool> &cancel,
                      unsigned node = 0)
      : transport_(t), cancelled_(cancel), node_(node) {
    if (node >= 15)
      throw std::runtime_error("Invalid repository node");
  }
  void detect();
  std::uint32_t repository_status() { return quick(0x40, 0x80); }
  RepositoryFile read(std::uint32_t selector);
};
std::vector<std::uint32_t> read_can_plan(const std::filesystem::path &);
} // namespace rnet
