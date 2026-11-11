#pragma once
#include "can_reader.hpp"
namespace rnet {
// Deliberately limited to the verified PID68 mapping (file 10006/v22, 623
// bytes). Compatibility validation for the original PID68-only helper.
void validate_pid68_change(const RepositoryFile &before,
                           const RepositoryFile &after);
class ControllerReadbackError : public std::runtime_error {
public:
  bool commit_acknowledged;
  ControllerReadbackError(const std::string &message,bool acknowledged)
      :std::runtime_error(message),commit_acknowledged(acknowledged){}
};
struct Pid68ProgramResult {
  RepositoryFile readback;
  bool changed = false, commit_acknowledged = false, readback_verified = false;
};
void validate_repository_change(const RepositoryFile&, const RepositoryFile&);
class CanRepositoryWriter {
  CanTransport &transport_;
  const std::atomic<bool> &cancelled_;
  unsigned node_;
  void release() noexcept;

public:
  CanRepositoryWriter(CanTransport &transport, const std::atomic<bool> &cancelled,
                 unsigned node = 2)
      : transport_(transport), cancelled_(cancelled), node_(node) {
    if (node != 2)
      throw std::runtime_error(
          "Repository programming is restricted to verified repository node 2");
  }
  // Backup callback must persist the entire prepared document before download.
  Pid68ProgramResult program(
      const RepositoryFile &expectedBefore, const RepositoryFile &after,
      const std::function<void(const RepositoryFile &, const RepositoryFile &)>
          &backup);
};
class CanPid68Writer {
  CanRepositoryWriter writer_;
public:
  CanPid68Writer(CanTransport &t, const std::atomic<bool> &c, unsigned node=2)
      : writer_(t,c,node) {}
  Pid68ProgramResult program(const RepositoryFile &before, const RepositoryFile &after,
      const std::function<void(const RepositoryFile&, const RepositoryFile&)> &backup) {
    validate_pid68_change(before,after);
    return writer_.program(before,after,backup);
  }
};
} // namespace rnet
