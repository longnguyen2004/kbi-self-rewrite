#include <recorder.h>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <libevdev/libevdev.h>

class recorder_linux_libevdev: public Recorder::Impl
{
public:
    bool Recording() const;
    void Start(bool kseyboard, bool mouse, bool gamepad);
    void Stop();
    const Recorder::DeviceMap& Devices() const;
    const Recorder::InputMap& Inputs() const;

private:
    bool m_running = false;
    Recorder::DeviceMap m_devices;
    Recorder::InputMap m_inputs;

    using EvdevDeviceMap = concurrent_flat_map<std::string, evdev_device>;
    EvdevDeviceMap m_evdev_devices;
    void _init_scan_devices();
    void _init_poll(bool keyboard, bool mouse, bool gamepad);
    std::jthread m_device_scan_thread;
    std::jthread m_poll_thread;
};