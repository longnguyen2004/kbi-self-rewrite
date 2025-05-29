#pragma once

#include <recorder.h>

class Recorder::Impl
{
public:
    using InputSignalWithVIDPID =
        boost::signals2::signal<void(const std::string&, std::uint16_t, std::uint16_t, Input)>;

    Impl() = default;
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    InputSignalWithVIDPID& OnInput()
    {
        return m_sig_input;
    }

    virtual bool Recording() const = 0;
    virtual void Start(bool keyboard = true, bool mouse = false, bool gamepad = false) = 0;
    virtual void Stop() = 0;

private:
    InputSignalWithVIDPID m_sig_input;
};
