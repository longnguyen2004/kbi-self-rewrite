#include "recorder/recorder_source.h"

#include <recorder.h>

#include <boost/signals2/connection.hpp>

#include <spdlog/spdlog.h>

#include <stdexcept>

namespace Kbi {

struct RecorderSource::Impl {
    std::unique_ptr<::Recorder> recorder;
    boost::signals2::scoped_connection usbConnection;
    boost::signals2::scoped_connection deviceConnection;
    boost::signals2::scoped_connection inputConnection;
};

RecorderSource::RecorderSource()
    : m_impl(std::make_unique<Impl>())
{
}

RecorderSource::~RecorderSource()
{
    stop();
}

bool RecorderSource::start(std::string& error)
{
    if (m_impl->recorder && m_impl->recorder->recording())
        return true;

    try {
        // The recorder dereferences the logger it is given, so always hand it
        // a valid one.
        m_impl->recorder = std::make_unique<::Recorder>(::RecorderBackend::AUTO,
            spdlog::default_logger());
    } catch (const std::exception& e) {
        error = e.what();
        m_impl->recorder.reset();
        return false;
    }

    {
        std::scoped_lock lock(m_mutex);
        m_queue.clear();
    }

    auto& recorder = *m_impl->recorder;

    m_impl->usbConnection = recorder.onUsbDevice().connect([this](const std::string& id, const UsbDeviceInfo& info) {
        Event event;
        event.type = EventType::UsbDevice;
        event.id = id;
        event.usbDevice.vid = info.vid;
        event.usbDevice.pid = info.pid;
        event.usbDevice.speed = static_cast<UsbDeviceSpeed>(static_cast<int>(info.speed));
        event.usbDevice.descriptors = info.descriptors;
        std::scoped_lock lock(m_mutex);
        m_queue.push_back(std::move(event));
    });

    m_impl->deviceConnection = recorder.onDevice().connect([this](const std::string& id, const ::Device& info) {
        Event event;
        event.type = EventType::Device;
        event.id = id;
        event.device.name = info.name;
        event.device.vid = info.vid;
        event.device.pid = info.pid;
        if (info.usbDeviceId)
            event.device.usbDevice = *info.usbDeviceId;
        std::scoped_lock lock(m_mutex);
        m_queue.push_back(std::move(event));
    });

    m_impl->inputConnection = recorder.onInput().connect([this](const std::string& id, const ::Input& info) {
        Event event;
        event.type = EventType::Input;
        event.id = id;
        event.input.timestamp = info.timestamp;
        event.input.pressed = info.pressed;
        event.input.code = static_cast<int>(info.code);
        std::scoped_lock lock(m_mutex);
        m_queue.push_back(std::move(event));
    });

    recorder.start(true, false, false);
    return true;
}

void RecorderSource::stop()
{
    if (!m_impl->recorder)
        return;
    if (m_impl->recorder->recording())
        m_impl->recorder->stop();
    m_impl->usbConnection.disconnect();
    m_impl->deviceConnection.disconnect();
    m_impl->inputConnection.disconnect();
    m_impl->recorder.reset();
}

bool RecorderSource::recording() const
{
    return m_impl->recorder && m_impl->recorder->recording();
}

const SystemInfo& RecorderSource::systemInfo() const
{
    static const SystemInfo empty;
    if (!m_impl->recorder)
        return empty;
    return m_impl->recorder->getSystemInfo();
}

std::string_view RecorderSource::backendName() const
{
    if (!m_impl->recorder)
        return {};
    switch (m_impl->recorder->backend()) {
    case ::RecorderBackend::WindowsGameinput:
        return "gameinput";
    case ::RecorderBackend::WindowsRawinput:
        return "rawinput";
    case ::RecorderBackend::LinuxEvdev:
        return "evdev";
    default:
        return "unknown";
    }
}

void RecorderSource::drain(Result& result, Analyzer& analyzer, InputTimeline& timeline)
{
    std::vector<Event> events;
    {
        std::scoped_lock lock(m_mutex);
        events.swap(m_queue);
    }
    if (events.empty())
        return;

    std::vector<double> timestamps;
    timestamps.reserve(events.size());

    for (const Event& event : events) {
        switch (event.type) {
        case EventType::UsbDevice:
            result.usbDevices[event.id] = event.usbDevice;
            break;
        case EventType::Device:
            result.addDevice(event.id, event.device);
            break;
        case EventType::Input:
            result.inputs[event.id].push_back(event.input);
            timestamps.push_back(event.input.timestamp);
            timeline.add(event.id, event.input);
            break;
        }
    }

    if (!timestamps.empty())
        analyzer.add(timestamps);
}

} // namespace Kbi
