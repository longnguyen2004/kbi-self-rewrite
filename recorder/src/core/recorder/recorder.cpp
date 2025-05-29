#include "recorder_impl.h"
#include <stdexcept>
#include <iostream>

#ifdef _WIN32
#include "win/impl.h"
#endif

Recorder::Recorder(RecorderBackend backend)
{
#ifdef _WIN32
    if (
        backend == RecorderBackend::AUTO ||
        backend == RecorderBackend::WINDOWS_GAMEINPUT
    )
    {
        try {
            p_impl = std::make_unique<recorder_win_gameinput>();
            m_backend = RecorderBackend::WINDOWS_GAMEINPUT;
        }
        catch (const std::exception& e) {

        }
    }
#endif

    if (!p_impl)
        throw std::runtime_error("Failed to initialize any backend");

    p_impl->OnInput().connect([this](
        const std::string& id, std::uint16_t vid, std::uint16_t pid, Input input
    ) {
        if (!m_devices.contains(id))
        {
            this->OnDevice()(id, m_devices.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(id),
                std::forward_as_tuple(vid, pid, _get_device_name(id))
            ).first->second);
        }
        m_inputs[id].push_back(input);
        this->OnInput()(id, input);
        m_input_count++;
    });
}

bool Recorder::Recording() const
{
    return p_impl->Recording();
}

void Recorder::Start(bool keyboard, bool mouse, bool gamepad)
{
    m_devices.clear();
    m_inputs.clear();
    p_impl->Start(keyboard, mouse, gamepad);
    m_start_time = std::chrono::steady_clock::now();
}

void Recorder::Stop()
{
    p_impl->Stop();
    m_end_time = std::chrono::steady_clock::now();
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
    return m_input_count;
}