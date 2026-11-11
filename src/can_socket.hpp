#pragma once
#include "can_reader.hpp"
#include <memory>
namespace rnet {
// Separate bounded capability for the single verified PID68 0/1 -> 1 plan.
// Validates each data segment and metadata value; no arbitrary CAN writes.
std::unique_ptr<CanTransport> open_can_pid68_transport(
    const std::string &interface, const std::filesystem::path &log,
    const RepositoryFile &before, const RepositoryFile &after);
std::unique_ptr<CanTransport> open_can_write_transport(
    const std::string&, const std::filesystem::path&,
    const RepositoryFile&, const RepositoryFile&);
// Ordinary read transport remains strictly read/release only.
std::unique_ptr<CanTransport>
open_can_read_transport(const std::string &interface,
                        const std::filesystem::path &log);
} // namespace rnet
