#pragma once

#include <device.h>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Kbi {

std::string_view toString(UsbDeviceSpeed speed);

// A single HID input event.
struct InputEvent {
    // Microseconds since the recording started. The legacy .kbi format stores
    // fractional microseconds, so this is not an integer.
    double timestamp = 0.0;
    bool pressed = false;
    int code = 0; // Keycode (HID usage id)
};

// A HID device that produced input events.
struct ResultDevice {
    std::string name;
    std::uint16_t vid = 0;
    std::uint16_t pid = 0;
    std::optional<std::string> usbDevice; // key into Result::usb_devices
};

// A USB device, optionally carrying the raw descriptor blob.
struct UsbDevice {
    std::uint16_t vid = 0;
    std::uint16_t pid = 0;
    UsbDeviceSpeed speed = UsbDeviceSpeed::Unknown;
    std::vector<unsigned char> descriptors;
};

struct RecordingInfo {
    std::string osName;
    std::string osVer;
    std::string arch;
    std::string backend;
    std::optional<bool> safeMode;
};

// A whole recording: metadata plus the per-device input streams.
struct Result {
    RecordingInfo info;
    std::string time;
    std::map<std::string, UsbDevice> usbDevices;
    // The devices in the order they were discovered, the colors of the input
    // timeline are assigned in that order.
    std::vector<std::pair<std::string, ResultDevice>> devices;
    std::map<std::string, std::vector<InputEvent>> inputs;

    // Adds a device, or updates it when it is already known.
    void addDevice(std::string id, ResultDevice device)
    {
        for (auto& [knownId, known] : devices) {
            if (knownId == id) {
                known = std::move(device);
                return;
            }
        }
        devices.emplace_back(std::move(id), std::move(device));
    }

    // The device with the given id, or nullptr when it is unknown.
    [[nodiscard]] const ResultDevice* findDevice(const std::string& id) const
    {
        for (const auto& [knownId, known] : devices) {
            if (knownId == id)
                return &known;
        }
        return nullptr;
    }

    [[nodiscard]] std::size_t totalInputCount() const
    {
        std::size_t count = 0;
        for (const auto& [id, events] : inputs)
            count += events.size();
        return count;
    }
};

} // namespace Kbi
