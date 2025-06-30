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
        !p_impl && (
            backend == RecorderBackend::AUTO ||
            backend == RecorderBackend::WINDOWS_GAMEINPUT
        )
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
    if (
        !p_impl && (
            backend == RecorderBackend::AUTO ||
            backend == RecorderBackend::WINDOWS_RAWINPUT
        )
    )
    {
        try {
            m_logger->info("Trying to initialize Raw Input backend");
            p_impl = std::make_unique<recorder_win_rawinput>(logger);
            m_backend = RecorderBackend::WINDOWS_RAWINPUT;
            m_logger->info("Initialized Raw Input backend");
        }
        catch (const std::exception& e) {
            m_logger->info("Failed to initialize Raw Input backend");
        }
    }
#endif

#ifdef __linux__
    if (
        !p_impl && (
            backend == RecorderBackend::AUTO ||
            backend == RecorderBackend::LINUX_EVDEV
        )
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
        m_logger->error("Failed to initialize any backend");
        throw std::runtime_error("Failed to initialize any backend");
    }

    p_impl->OnInput().connect([this](
        const std::string& id, std::uint16_t vid, std::uint16_t pid, Input input
    ) {
        m_devices.try_emplace_and_visit(
            id,
            ""s, vid, pid, std::nullopt,
            [&, this](DeviceMap::value_type& new_device) {
                new_device.second.Name = p_impl->GetDeviceName(id);
                auto usbDevice = p_impl->GetUsbDeviceId(id);
                if (usbDevice)
                {
                    new_device.second.UsbDeviceId = usbDevice;
                    m_usb_devices.try_emplace_and_visit(
                        usbDevice.value(), std::nullopt,
                        [&, this](UsbDeviceMap::value_type& new_usb_device)
                        {
                            auto& [id, device] = new_usb_device;
                            device = std::move(p_impl->GetUsbDeviceInfo(id));
                            if (device)
                                this->OnUsbDevice()(id, device.value());
                        },
                        [](auto& _)
                        {
                        }
                    );
                }
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
    m_logger->debug("Started recording");
    OnStart()();
}

void Recorder::Stop()
{
    if (!m_running)
        return;
    p_impl->Stop();
    m_end_time = std::chrono::steady_clock::now();
    m_running = false;
    m_logger->debug("Stopped recording");
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

const Recorder::UsbDeviceMap& Recorder::UsbDevices() const
{
    return m_usb_devices;
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
