#pragma once

#include <device.h>
#include <keycode.h>
#include <memory>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
#include <deque>
#include <utility>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/signals2.hpp>
#include <spdlog/fwd.h>

struct Input {
    std::uint64_t Timestamp;
    bool Pressed;
    Keycode Code;
};

enum class RecorderBackend {
    AUTO,
    WINDOWS_GAMEINPUT,
    WINDOWS_RAWINPUT,
    LINUX_EVDEV
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

    Recorder(RecorderBackend backend = RecorderBackend::AUTO, std::shared_ptr<spdlog::logger> logger = nullptr);
    ~Recorder();

    UsbDeviceSignal& OnUsbDevice()
    {
        return m_sig_usb_device;
    }
    DeviceSignal& OnDevice()
    {
        return m_sig_device;
    }
    InputSignal& OnInput()
    {
        return m_sig_input;
    }
    StartSignal& OnStart()
    {
        return m_sig_start;
    }
    StopSignal& OnStop()
    {
        return m_sig_stop;
    }

    bool Recording() const;
    void Start(bool keyboard = true, bool mouse = false, bool gamepad = false);
    void Stop();

    std::chrono::system_clock::time_point StartTime() const;
    std::chrono::steady_clock::duration Elapsed() const;

    RecorderBackend Backend() const;
    const UsbDeviceMap& UsbDevices() const;
    const DeviceMap& Devices() const;
    const InputMap& Inputs() const;
    size_t DeviceCount() const;
    size_t InputCount() const;

private:
    bool m_running = false;

    std::unique_ptr<Impl> p_impl;
    std::shared_ptr<spdlog::logger> m_logger;
    RecorderBackend m_backend;
    UsbDeviceMap m_usb_devices;
    DeviceMap m_devices;
    InputMap m_inputs;
    UsbDeviceSignal m_sig_usb_device;
    DeviceSignal m_sig_device;
    InputSignal m_sig_input;
    StartSignal m_sig_start;
    StopSignal m_sig_stop;

    std::chrono::system_clock::time_point m_start_wallclock;
    std::chrono::steady_clock::time_point m_start_time, m_end_time;
};
