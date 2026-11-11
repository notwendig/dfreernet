#pragma once
#include "can_reader.hpp"
#include <memory>
namespace rnet {
// Direct loopback RNC1 bridge. No FTDI/USB/DongleInterface calls.
std::unique_ptr<CanTransport> open_can_emulator_transport(const std::filesystem::path& log);
// Receive-only physical CAN bridge; send() rejects every CAN frame.
std::unique_ptr<CanTransport> open_can_monitor_transport(const std::filesystem::path& log);
}
