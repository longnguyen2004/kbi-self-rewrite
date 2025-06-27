#pragma once

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
    uint16_t VID;
    uint16_t PID;
    UsbDeviceSpeed Speed;
    std::vector<unsigned char> DeviceDescriptor;
    std::vector<unsigned char> ConfigDescriptor;
};

struct Device {
    std::string Name;
    uint16_t VID;
    uint16_t PID;
    std::optional<std::string> UsbDeviceId;
};
