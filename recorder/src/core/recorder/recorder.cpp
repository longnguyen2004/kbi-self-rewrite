#include "recorder_impl.h"
#include <stdexcept>
#include <iostream>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include "win/impl.h"
#endif

#ifdef __linux__
#include "linux/impl.h"
#endif

using namespace std::literals;

Recorder::Recorder(RecorderBackend backend, std::shared_ptr<spdlog::logger> logger):
    m_logger(logger ? logger : spdlog::default_logger())
{
#ifdef _WIN32
    if (
        backend == RecorderBackend::AUTO ||
        backend == RecorderBackend::WINDOWS_GAMEINPUT
    )
    {
        try {
            m_logger->info("Trying to initialize GameInput backend");
            p_impl = std::make_unique<recorder_win_gameinput>(logger);
            m_backend = RecorderBackend::WINDOWS_GAMEINPUT;
            m_logger->info("Initialized GameInput backend");
        }
        catch (const std::exception& e) {
            m_logger->info("Failed to initialize GameInput backend");
        }
    }
#endif

#ifdef __linux__
    if (
        backend == RecorderBackend::AUTO ||
        backend == RecorderBackend::LINUX_EVDEV
    )
    {
        try {
            m_logger->info("Trying to initialize evdev backend");
            p_impl = std::make_unique<recorder_linux_libevdev>(logger);
            m_backend = RecorderBackend::LINUX_EVDEV;
            m_logger->info("Initialized evdev backend");
        }
        catch (const std::exception& e) {
            m_logger->info("Failed to initialize evdev backend");
        }
    }
#endif

    if (!p_impl)
    {
        m_logger->warn("Failed to initialize any backend");
        throw std::runtime_error("Failed to initialize any backend");
    }

    p_impl->OnInput().connect([this](
        const std::string& id, std::uint16_t vid, std::uint16_t pid, Input input
    ) {
        m_devices.try_emplace_and_visit(
            id, vid, pid, ""s,
            [&, this](DeviceMap::value_type& new_device) {
                new_device.second.Name = p_impl->GetDeviceName(id);
                this->OnDevice()(id, new_device.second);
            },
            [](const DeviceMap::value_type& existing_device) {

            }
        );
        auto process_input = [&, this](InputMap::value_type& input_arr) {
            this->OnInput()(id, input);
            input_arr.second.push_back(input);
        };
        m_inputs.try_emplace_and_visit(
            id, 0, process_input, process_input
        );
    });
}

Recorder::~Recorder() = default;

bool Recorder::Recording() const
{
    return m_running;
}

void Recorder::Start(bool keyboard, bool mouse, bool gamepad)
{
    if (m_running)
        return;
    m_running = true;
    m_devices.clear();
    m_inputs.clear();
    p_impl->Start(keyboard, mouse, gamepad);
    m_start_time = std::chrono::steady_clock::now();
    m_start_wallclock = std::chrono::system_clock::now();
    OnStart()();
}

void Recorder::Stop()
{
    if (!m_running)
        return;
    p_impl->Stop();
    m_end_time = std::chrono::steady_clock::now();
    m_running = false;
    OnStop()();
}

std::chrono::system_clock::time_point Recorder::StartTime() const
{
    return m_start_wallclock;
}

std::chrono::steady_clock::duration Recorder::Elapsed() const
{
    auto end = Recording() ? std::chrono::steady_clock::now() : m_end_time;
    return end - m_start_time;
}

RecorderBackend Recorder::Backend() const
{
    return m_backend;
}

const Recorder::DeviceMap& Recorder::Devices() const
{
    return m_devices;
}

const Recorder::InputMap& Recorder::Inputs() const
{
    return m_inputs;
}

size_t Recorder::DeviceCount() const
{
    return m_devices.size();
}

size_t Recorder::InputCount() const
{
    size_t count = 0;
    m_inputs.visit_all([&](const InputMap::value_type& input) {
        count += input.second.size();
    });
    return count;
}
