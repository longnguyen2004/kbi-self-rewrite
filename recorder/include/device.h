#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Kbi {

// The absolute value is serialized into the .kbi and the JSON files, it has
// to stay stable.
enum class UsbDeviceSpeed {
    Unknown = 0,
    LowSpeed = 1,
    FullSpeed = 2,
    HighSpeed = 3,
    SuperSpeed = 4,
};

struct UsbDeviceInfo {
    std::uint16_t vid;
    std::uint16_t pid;
    UsbDeviceSpeed speed;
    std::vector<unsigned char> descriptors;
};

struct Device {
    std::string name;
    std::uint16_t vid;
    std::uint16_t pid;
    std::optional<std::string> usbDeviceId;
};

} // namespace Kbi
