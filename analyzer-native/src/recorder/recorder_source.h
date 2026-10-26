#pragma once

#include "core/analyzer.h"
#include "core/input_timeline.h"
#include "core/result.h"

#include <system_info.h>
#include <spdlog/fwd.h>

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Kbi {

// Wraps the recorder library, which is linked into the app instead of being
// spawned as an external process.
//
// The recorder signals fire on its own polling threads, so the events are
// queued and drained on the UI thread.
class RecorderSource {
public:
    explicit RecorderSource(std::shared_ptr<spdlog::logger> logger);
    ~RecorderSource();

    RecorderSource(const RecorderSource&) = delete;
    RecorderSource& operator=(const RecorderSource&) = delete;
    RecorderSource(RecorderSource&&) = delete;
    RecorderSource& operator=(RecorderSource&&) = delete;

    // Starts recording. On failure `error` receives a human readable reason.
    bool start(std::string& error);
    void stop();

    [[nodiscard]] bool recording() const;
    [[nodiscard]] std::string_view backendName() const;

    // The information about the machine, it comes from the recorder.
    [[nodiscard]] const SystemInfo& systemInfo() const;

    // Moves the events that arrived since the last call into `result` and
    // feeds the analyzer and the input timeline.
    void drain(Result& result, Analyzer& analyzer, InputTimeline& timeline);

private:
    enum class EventType { UsbDevice, Device, Input };

    struct Event {
        EventType type = EventType::Input;
        std::string id;
        UsbDevice usbDevice;
        ResultDevice device;
        InputEvent input;
    };

    struct Impl;

    std::unique_ptr<Impl> m_impl;
    std::mutex m_mutex;
    std::vector<Event> m_queue;
};

} // namespace Kbi
