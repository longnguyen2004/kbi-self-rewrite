#pragma once

#include <keycode.h>
#include <memory>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/signals2.hpp>

struct Device {
    std::uint16_t VID;
    std::uint16_t PID;
    std::string Name;
};

struct Input {
    std::uint64_t Timestamp;
    bool Pressed;
    Keycode Code;
};

enum class RecorderBackend {
    AUTO,
    WINDOWS_GAMEINPUT,
    LINUX_EVDEV
};

class Recorder {
public:
    class Impl;
    using DeviceMap = boost::unordered::concurrent_flat_map<std::string, Device>;
    using InputMap = boost::unordered::concurrent_flat_map<std::string, std::vector<Input>>;
    using DeviceSignal = boost::signals2::signal<void(const std::string&, Device)>;
    using InputSignal = boost::signals2::signal<void(const std::string&, Input)>;
    using StartSignal = boost::signals2::signal<void()>;
    using StopSignal = boost::signals2::signal<void()>;

    Recorder(RecorderBackend backend = RecorderBackend::AUTO);
    ~Recorder();

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
    const DeviceMap& Devices() const;
    const InputMap& Inputs() const;
    size_t DeviceCount() const;
    size_t InputCount() const;

private:
    bool m_running = false;

    std::unique_ptr<Impl> p_impl;
    RecorderBackend m_backend;
    DeviceMap m_devices;
    InputMap m_inputs;
    DeviceSignal m_sig_device;
    InputSignal m_sig_input;
    StartSignal m_sig_start;
    StopSignal m_sig_stop;

    std::chrono::system_clock::time_point m_start_wallclock;
    std::chrono::steady_clock::time_point m_start_time, m_end_time;
};
