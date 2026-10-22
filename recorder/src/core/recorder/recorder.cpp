#include "recorder_impl.h"
#include <iostream>
#include <spdlog/spdlog.h>
#include <stdexcept>

#ifdef _WIN32
#include "win/impl.h"
#endif

#ifdef __linux__
#include "linux/impl.h"
#endif

using namespace std::literals;

Recorder::Recorder(RecorderBackend backend, const std::shared_ptr<spdlog::logger>& logger)
    : m_logger(logger ? logger : spdlog::default_logger())
{
    m_systemInfo = ::getSystemInfo();

#ifdef _WIN32
    if (
        !p_impl && (backend == RecorderBackend::AUTO || backend == RecorderBackend::WINDOWS_GAMEINPUT)) {
        try {
            m_logger->info("Trying to initialize GameInput backend");
            p_impl = std::make_unique<RecorderWinGameinput>(logger);
            m_backend = RecorderBackend::WINDOWS_GAMEINPUT;
            m_logger->info("Initialized GameInput backend");
        } catch (const std::exception& e) {
            m_logger->info("Failed to initialize GameInput backend");
        }
    }
    if (
        !p_impl && (backend == RecorderBackend::AUTO || backend == RecorderBackend::WINDOWS_RAWINPUT)) {
        try {
            m_logger->info("Trying to initialize Raw Input backend");
            p_impl = std::make_unique<RecorderWinRawinput>(logger);
            m_backend = RecorderBackend::WINDOWS_RAWINPUT;
            m_logger->info("Initialized Raw Input backend");
        } catch (const std::exception& e) {
            m_logger->info("Failed to initialize Raw Input backend");
        }
    }
#endif

#ifdef __linux__
    if (
        !m_pImpl && (backend == RecorderBackend::AUTO || backend == RecorderBackend::LinuxEvdev)) {
        try {
            m_logger->info("Trying to initialize evdev backend");
            m_pImpl = std::make_unique<RecorderLinuxLibevdev>(logger);
            m_backend = RecorderBackend::LinuxEvdev;
            m_logger->info("Initialized evdev backend");
        } catch (const std::exception& e) {
            m_logger->info("Failed to initialize evdev backend");
        }
    }
#endif

    if (!m_pImpl) {
        m_logger->error("Failed to initialize any backend");
        throw std::runtime_error("Failed to initialize any backend");
    }

    m_pImpl->onInput().connect([this](
                                   const std::string& id, std::uint16_t vid, std::uint16_t pid, Input input) {
        m_devices.try_emplace_and_visit(
            id,
            ""s, vid, pid, std::nullopt,
            [&, this](DeviceMap::value_type& newDevice) {
                newDevice.second.name = m_pImpl->getDeviceName(id);
                auto usbDevice = m_pImpl->getUsbDeviceId(id);
                if (usbDevice) {
                    newDevice.second.usbDeviceId = usbDevice;
                    m_usbDevices.try_emplace_and_visit(
                        usbDevice.value(), std::nullopt,
                        [&, this](UsbDeviceMap::value_type& newUsbDevice) {
                            auto& [id, device] = newUsbDevice;
                            device = std::move(m_pImpl->getUsbDeviceInfo(id));
                            if (device)
                                this->onUsbDevice()(id, device.value());
                        },
                        [](auto& _) {
                        });
                }
                this->onDevice()(id, newDevice.second);
            },
            [](const DeviceMap::value_type& existingDevice) {

            });
        auto processInput = [&, this](InputMap::value_type& inputArr) {
            this->onInput()(id, input);
            inputArr.second.push_back(input);
        };
        m_inputs.try_emplace_and_visit(
            id, 0, processInput, processInput);
    });
}

Recorder::~Recorder() = default;

bool Recorder::recording() const
{
    return m_running;
}

void Recorder::start(bool keyboard, bool mouse, bool gamepad)
{
    if (m_running)
        return;
    m_running = true;
    m_devices.clear();
    m_inputs.clear();
    m_pImpl->start(keyboard, mouse, gamepad);
    m_startTime = std::chrono::steady_clock::now();
    m_startWallclock = std::chrono::system_clock::now();
    m_logger->debug("Started recording");
    onStart()();
}

void Recorder::stop()
{
    if (!m_running)
        return;
    m_pImpl->stop();
    m_endTime = std::chrono::steady_clock::now();
    m_running = false;
    m_logger->debug("Stopped recording");
    onStop()();
}

std::chrono::system_clock::time_point Recorder::startTime() const
{
    return m_startWallclock;
}

std::chrono::steady_clock::duration Recorder::elapsed() const
{
    auto end = recording() ? std::chrono::steady_clock::now() : m_endTime;
    return end - m_startTime;
}

RecorderBackend Recorder::backend() const
{
    return m_backend;
}

const Recorder::UsbDeviceMap& Recorder::usbDevices() const
{
    return m_usbDevices;
}

const Recorder::DeviceMap& Recorder::devices() const
{
    return m_devices;
}

const Recorder::InputMap& Recorder::inputs() const
{
    return m_inputs;
}

size_t Recorder::deviceCount() const
{
    return m_devices.size();
}

size_t Recorder::inputCount() const
{
    size_t count = 0;
    m_inputs.visit_all([&](const InputMap::value_type& input) {
        count += input.second.size();
    });
    return count;
}
