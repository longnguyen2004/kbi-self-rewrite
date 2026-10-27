#pragma once

#include <recorder.h>
#include <spdlog/fwd.h>

#include <utility>

namespace Kbi {

// The platform specific implementation of the `Recorder`.
class Recorder::Impl {
public:
    using InputSignalWithVIDPID
        = boost::signals2::signal<void(const std::string&, const VidPid&, Input)>;

    explicit Impl(std::shared_ptr<spdlog::logger> logger)
        : m_logger(std::move(logger))
    {
    }
    virtual ~Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    InputSignalWithVIDPID& onInput() { return m_sigInput; }

    virtual void start(InputMask devices) = 0;
    virtual void stop() = 0;
    virtual std::string deviceName(std::string_view id) const = 0;
    virtual std::optional<std::string> usbDeviceId(std::string_view id) const = 0;
    virtual std::optional<UsbDeviceInfo> usbDeviceInfo(std::string_view id) const = 0;

protected:
    // The backends log through this accessor (C.133 — the data member itself
    // is private).
    [[nodiscard]] const std::shared_ptr<spdlog::logger>& logger() const { return m_logger; }

private:
    std::shared_ptr<spdlog::logger> m_logger;
    InputSignalWithVIDPID m_sigInput;
};

} // namespace Kbi
