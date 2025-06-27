#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

enum class UsbDeviceSpeed {
    UNKNOWN,
    LOW_SPEED,
    FULL_SPEED,
    HIGH_SPEED,
    SUPERSPEED
};

struct UsbDeviceInfo {
    std::uint16_t VID;
    std::uint16_t PID;
    UsbDeviceSpeed Speed;
    std::vector<unsigned char> Descriptors;
};

struct Device {
    std::string Name;
    std::uint16_t VID;
    std::uint16_t PID;
    std::optional<std::string> UsbDeviceId;
};
