#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
namespace rnet {
struct DeviceInfo {std::string serial,description;};
struct DeviceScan {unsigned long status=0,library_version=0,library_status=0;bool enumeration_attempted=false;std::vector<DeviceInfo> devices;};
class DeviceDiscovery {
public:
    // Enumeration only: no handle opened, no CAN/USB payload transmitted.
    DeviceScan scan(const std::filesystem::path& original_library) const;
};
}
