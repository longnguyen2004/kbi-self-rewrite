#pragma once

#include <device.h>
#include <keycode.h>
#include <system_info.h>
#include <atomic>
#include <boost/signals2.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <spdlog/fwd.h>
#include <string>
#include <string_view>
#include <utility>

struct Input {
    std::uint64_t timestamp;
    bool pressed;
    Keycode code;
};

enum class RecorderBackend {
    AUTO,
    WindowsGameinput,
    WindowsRawinput,
    LinuxEvdev
};

class Recorder {
public:
    class Impl;
    using UsbDeviceMap = boost::unordered::concurrent_flat_map<std::string, std::optional<UsbDeviceInfo>>;
    using DeviceMap = boost::unordered::concurrent_flat_map<std::string, Device>;
    using InputMap = boost::unordered::concurrent_flat_map<std::string, std::deque<Input>>;
    using UsbDeviceSignal = boost::signals2::signal<void(const std::string&, const UsbDeviceInfo&)>;
    using DeviceSignal = boost::signals2::signal<void(const std::string&, const Device&)>;
    using InputSignal = boost::signals2::signal<void(const std::string&, const Input&)>;
    using StartSignal = boost::signals2::signal<void()>;
    using StopSignal = boost::signals2::signal<void()>;

    explicit Recorder(RecorderBackend backend = RecorderBackend::AUTO, const std::shared_ptr<spdlog::logger>& logger = nullptr);
    ~Recorder();

    UsbDeviceSignal& onUsbDevice()
    {
        return m_sigUsbDevice;
    }
    DeviceSignal& onDevice()
    {
        return m_sigDevice;
    }
    InputSignal& onInput()
    {
        return m_sigInput;
    }
    StartSignal& onStart()
    {
        return m_sigStart;
    }
    StopSignal& onStop()
    {
        return m_sigStop;
    }

    bool recording() const;
    void start(bool keyboard = true, bool mouse = false, bool gamepad = false);
    void stop();

    std::chrono::system_clock::time_point startTime() const;
    std::chrono::steady_clock::duration elapsed() const;

    RecorderBackend backend() const;

    // The information about the machine, collected when the recorder was
    // created.
    const SystemInfo& getSystemInfo() const { return m_systemInfo; }

    const UsbDeviceMap& usbDevices() const;
    const DeviceMap& devices() const;
    const InputMap& inputs() const;
    size_t deviceCount() const;
    size_t inputCount() const;

private:
    bool m_running = false;

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
    std::chrono::steady_clock::time_point m_startTime, m_endTime;
};
