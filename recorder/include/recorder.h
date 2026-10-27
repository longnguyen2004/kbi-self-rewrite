#pragma once

#include "device.h"
#include "keycode.h"
#include "system_info.h"
#include <atomic>
#include <boost/signals2.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <spdlog/fwd.h>
#include <string>
#include <string_view>
#include <utility>

namespace Kbi {

struct Input {
    std::uint64_t timestamp;
    bool pressed;
    Keycode code;
};

// The vendor and product id scraped from a device's PnP path. One strong
// type instead of two adjacent, swappable integers (I.24).
struct VidPid {
    std::uint16_t vid = 0;
    std::uint16_t pid = 0;
};

// The device classes a recording captures. A bitmask, so callers state what
// they want instead of passing three adjacent bools (I.4).
enum class InputMask : unsigned {
    None = 0,
    Keyboard = 1u << 0,
    Mouse = 1u << 1,
    Gamepad = 1u << 2,
};

[[nodiscard]] constexpr InputMask operator|(InputMask a, InputMask b)
{
    return static_cast<InputMask>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

[[nodiscard]] constexpr bool has(InputMask mask, InputMask flag)
{
    return (static_cast<unsigned>(mask) & static_cast<unsigned>(flag)) != 0;
}

enum class RecorderBackend {
    Auto,
    WindowsGameInput,
    WindowsRawInput,
    LinuxEvdev,
};

class Recorder {
public:
    class Impl;
    using UsbDeviceMap
        = boost::unordered::concurrent_flat_map<std::string, std::optional<UsbDeviceInfo>>;
    using DeviceMap = boost::unordered::concurrent_flat_map<std::string, Device>;
    using InputMap = boost::unordered::concurrent_flat_map<std::string, std::deque<Input>>;
    using UsbDeviceSignal = boost::signals2::signal<void(const std::string&, const UsbDeviceInfo&)>;
    using DeviceSignal = boost::signals2::signal<void(const std::string&, const Device&)>;
    using InputSignal = boost::signals2::signal<void(const std::string&, const Input&)>;
    using StartSignal = boost::signals2::signal<void()>;
    using StopSignal = boost::signals2::signal<void()>;

    // The logger is taken by value: under Boost.Ext.DI's lazy constructor
    // argument conversion, a `const std::shared_ptr&` parameter binds to a
    // reference into a temporary wrapper that is destroyed before the
    // constructor runs (dangling copy and a crash in _Ref_count_base::_Incref).
    explicit Recorder(RecorderBackend backend = RecorderBackend::Auto,
        std::shared_ptr<spdlog::logger> logger = nullptr);
    ~Recorder();

    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;
    Recorder(Recorder&&) = delete;
    Recorder& operator=(Recorder&&) = delete;

    UsbDeviceSignal& onUsbDevice() { return m_sigUsbDevice; }
    DeviceSignal& onDevice() { return m_sigDevice; }
    InputSignal& onInput() { return m_sigInput; }
    StartSignal& onStart() { return m_sigStart; }
    StopSignal& onStop() { return m_sigStop; }

    // Whether inputs are being recorded.
    [[nodiscard]] bool recording() const;
    // Starts recording the given device classes.
    void start(InputMask devices = InputMask::Keyboard);
    void stop();

    // The wall clock time the last recording started at.
    [[nodiscard]] std::chrono::system_clock::time_point startTime() const;
    // The time the last recording took, ongoing recordings count up.
    [[nodiscard]] std::chrono::steady_clock::duration elapsed() const;

    // The backend the recorder was initialized with. With `RecorderBackend::
    // Auto` it is the backend that was picked.
    [[nodiscard]] RecorderBackend backend() const;

    // The information about the machine, collected when the recorder was
    // created.
    [[nodiscard]] const SystemInfo& systemInfo() const { return m_systemInfo; }

    [[nodiscard]] const UsbDeviceMap& usbDevices() const;
    [[nodiscard]] const DeviceMap& devices() const;
    [[nodiscard]] const InputMap& inputs() const;
    [[nodiscard]] std::size_t deviceCount() const;
    [[nodiscard]] std::size_t inputCount() const;

private:
    std::atomic<bool> m_running { false };

    std::unique_ptr<Impl> m_pImpl;
    std::shared_ptr<spdlog::logger> m_logger;
    RecorderBackend m_backend;
    SystemInfo m_systemInfo;
    UsbDeviceMap m_usbDevices;
    DeviceMap m_devices;
    InputMap m_inputs;
    UsbDeviceSignal m_sigUsbDevice;
    DeviceSignal m_sigDevice;
    InputSignal m_sigInput;
    StartSignal m_sigStart;
    StopSignal m_sigStop;

    std::chrono::system_clock::time_point m_startWallclock;
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_endTime;
};

} // namespace Kbi
