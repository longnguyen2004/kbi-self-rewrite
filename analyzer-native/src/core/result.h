#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace kbi {

// Mirrors the `UsbDeviceSpeed` ordinals used by the recorder and by the
// analyzer's JSON schema.
enum class UsbDeviceSpeed {
    Unknown = 0,
    LowSpeed = 1,
    FullSpeed = 2,
    HighSpeed = 3,
    SuperSpeed = 4,
};

std::string_view to_string(UsbDeviceSpeed speed);

// A single HID input event.
struct InputEvent {
    // Microseconds since the recording started. The legacy .kbi format stores
    // fractional microseconds, so this is not an integer.
    double timestamp = 0.0;
    bool pressed = false;
    int code = 0; // Keycode (HID usage id)
};

// A HID device that produced input events.
struct Device {
    std::string name;
    std::uint16_t vid = 0;
    std::uint16_t pid = 0;
    std::optional<std::string> usb_device; // key into Result::usb_devices
};

// A USB device, optionally carrying the raw descriptor blob.
struct UsbDevice {
    std::uint16_t vid = 0;
    std::uint16_t pid = 0;
    UsbDeviceSpeed speed = UsbDeviceSpeed::Unknown;
    std::vector<unsigned char> descriptors;
};

struct RecordingInfo {
    std::string os_name;
    std::string os_ver;
    std::string arch;
    std::string backend;
    std::optional<bool> safe_mode;
};

// A whole recording: metadata plus the per-device input streams.
struct Result {
    RecordingInfo info;
    std::string time;
    std::map<std::string, UsbDevice> usb_devices;
    std::map<std::string, Device> devices;
    std::map<std::string, std::vector<InputEvent>> inputs;

    [[nodiscard]] std::size_t total_input_count() const {
        std::size_t count = 0;
        for (const auto& [id, events] : inputs)
            count += events.size();
        return count;
    }
};

} // namespace kbi
