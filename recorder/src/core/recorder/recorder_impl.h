#pragma once

#include <recorder.h>
#include <spdlog/fwd.h>

#include <utility>

class Recorder::Impl {
public:
    using InputSignalWithVIDPID = boost::signals2::signal<void(const std::string&, std::uint16_t, std::uint16_t, Input)>;

    explicit Impl(std::shared_ptr<spdlog::logger> logger)
        : m_logger(std::move(logger))
    {
    }
    virtual ~Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    InputSignalWithVIDPID& onInput()
    {
        return m_sigInput;
    }

    virtual void start(bool keyboard = true, bool mouse = false, bool gamepad = false) = 0;
    virtual void stop() = 0;
    virtual std::string getDeviceName(std::string_view id) const = 0;
    virtual std::optional<std::string> getUsbDeviceId(std::string_view id) const = 0;
    virtual std::optional<UsbDeviceInfo> getUsbDeviceInfo(std::string_view id) const = 0;

protected:
    std::shared_ptr<spdlog::logger> m_logger;

private:
    InputSignalWithVIDPID m_sigInput;
};
