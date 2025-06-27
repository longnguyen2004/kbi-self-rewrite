#pragma once

#include <recorder.h>
#include <spdlog/fwd.h>

class Recorder::Impl
{
public:
    using InputSignalWithVIDPID =
        boost::signals2::signal<void(const std::string&, std::uint16_t, std::uint16_t, Input)>;

    Impl(std::shared_ptr<spdlog::logger> logger): m_logger(logger) {}
    virtual ~Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    InputSignalWithVIDPID& OnInput()
    {
        return m_sig_input;
    }

    virtual void Start(bool keyboard = true, bool mouse = false, bool gamepad = false) = 0;
    virtual void Stop() = 0;
    virtual std::string GetDeviceName(std::string_view id) const = 0;
    virtual std::optional<std::string> GetUsbDeviceId(std::string_view id) const = 0;
    virtual std::optional<UsbDeviceInfo> GetUsbDeviceInfo(std::string_view id) const = 0;

protected:
    std::shared_ptr<spdlog::logger> m_logger;

private:
    InputSignalWithVIDPID m_sig_input;
};
