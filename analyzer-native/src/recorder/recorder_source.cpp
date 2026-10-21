#include "recorder/recorder_source.h"

#include <recorder.h>

#include <boost/signals2/connection.hpp>

#include <spdlog/spdlog.h>

#include <stdexcept>

namespace kbi {

struct RecorderSource::Impl {
    std::unique_ptr<::Recorder> recorder;
    boost::signals2::scoped_connection usb_connection;
    boost::signals2::scoped_connection device_connection;
    boost::signals2::scoped_connection input_connection;
};

RecorderSource::RecorderSource() : m_impl(std::make_unique<Impl>()) {}

RecorderSource::~RecorderSource() {
    stop();
}

bool RecorderSource::start(std::string& error) {
    if (m_impl->recorder && m_impl->recorder->Recording())
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
        std::lock_guard lock(m_mutex);
        m_queue.clear();
    }

    auto& recorder = *m_impl->recorder;

    m_impl->usb_connection =
        recorder.OnUsbDevice().connect([this](const std::string& id, const UsbDeviceInfo& info) {
            Event event;
            event.type = EventType::UsbDevice;
            event.id = id;
            event.usb_device.vid = info.VID;
            event.usb_device.pid = info.PID;
            event.usb_device.speed = static_cast<UsbDeviceSpeed>(static_cast<int>(info.Speed));
            event.usb_device.descriptors = info.Descriptors;
            std::lock_guard lock(m_mutex);
            m_queue.push_back(std::move(event));
        });

    m_impl->device_connection =
        recorder.OnDevice().connect([this](const std::string& id, const ::Device& info) {
            Event event;
            event.type = EventType::Device;
            event.id = id;
            event.device.name = info.Name;
            event.device.vid = info.VID;
            event.device.pid = info.PID;
            if (info.UsbDeviceId)
                event.device.usb_device = *info.UsbDeviceId;
            std::lock_guard lock(m_mutex);
            m_queue.push_back(std::move(event));
        });

    m_impl->input_connection =
        recorder.OnInput().connect([this](const std::string& id, const ::Input& info) {
            Event event;
            event.type = EventType::Input;
            event.id = id;
            event.input.timestamp = info.Timestamp;
            event.input.pressed = info.Pressed;
            event.input.code = static_cast<int>(info.Code);
            std::lock_guard lock(m_mutex);
            m_queue.push_back(std::move(event));
        });

    recorder.Start(true, false, false);
    return true;
}

void RecorderSource::stop() {
    if (!m_impl->recorder)
        return;
    if (m_impl->recorder->Recording())
        m_impl->recorder->Stop();
    m_impl->usb_connection.disconnect();
    m_impl->device_connection.disconnect();
    m_impl->input_connection.disconnect();
    m_impl->recorder.reset();
}

bool RecorderSource::recording() const {
    return m_impl->recorder && m_impl->recorder->Recording();
}

const SystemInfo& RecorderSource::system_info() const {
    static const SystemInfo empty;
    if (!m_impl->recorder)
        return empty;
    return m_impl->recorder->GetSystemInfo();
}

std::string RecorderSource::backend_name() const {
    if (!m_impl->recorder)
        return {};
    switch (m_impl->recorder->Backend()) {
    case ::RecorderBackend::WINDOWS_GAMEINPUT:
        return "gameinput";
    case ::RecorderBackend::WINDOWS_RAWINPUT:
        return "rawinput";
    case ::RecorderBackend::LINUX_EVDEV:
        return "evdev";
    default:
        return "unknown";
    }
}

void RecorderSource::drain(Result& result, Analyzer& analyzer, InputTimeline& timeline) {
    std::vector<Event> events;
    {
        std::lock_guard lock(m_mutex);
        events.swap(m_queue);
    }
    if (events.empty())
        return;

    std::vector<double> timestamps;
    timestamps.reserve(events.size());

    for (const Event& event : events) {
        switch (event.type) {
        case EventType::UsbDevice:
            result.usb_devices[event.id] = event.usb_device;
            break;
        case EventType::Device:
            result.devices[event.id] = event.device;
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

} // namespace kbi
