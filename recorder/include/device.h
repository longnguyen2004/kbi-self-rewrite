#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class UsbDeviceSpeed {
    UNKNOWN,
    LowSpeed,
    FullSpeed,
    HighSpeed,
    SUPERSPEED
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
