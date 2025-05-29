#pragma once

#include <keycode.h>
#include <memory>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <utility>
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
    using DeviceMap = std::unordered_map<std::string, Device>;
    using InputMap = std::unordered_map<std::string, std::vector<Input>>;
    using DeviceSignal = boost::signals2::signal<void(const std::string&, Device)>;
    using InputSignal = boost::signals2::signal<void(const std::string&, Input)>;

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

    bool Recording() const;
    void Start(bool keyboard = true, bool mouse = false, bool gamepad = false);
    void Stop();
    std::chrono::steady_clock::duration Elapsed() const;

    RecorderBackend Backend() const;
    const DeviceMap& Devices() const;
    const InputMap& Inputs() const;
    size_t DeviceCount() const;
    size_t InputCount() const;

private:
    bool m_running = false;

    std::string _get_device_name(const std::string& id);
    std::unique_ptr<Impl> p_impl;
    RecorderBackend m_backend;
    DeviceMap m_devices;
    InputMap m_inputs;
    std::atomic<size_t> m_input_count = 0;
    DeviceSignal m_sig_device;
    InputSignal m_sig_input;

    std::chrono::steady_clock::time_point m_start_time, m_end_time;
};
