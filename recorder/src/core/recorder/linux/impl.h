#include "../recorder_impl.h"
#include <thread>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <libevdev/libevdev.h>

struct evdev_device
{
    libevdev* dev = nullptr;
    bool remove = false;
};

class recorder_linux_libevdev: public Recorder::Impl
{
public:
    recorder_linux_libevdev();
    virtual void Start(bool kseyboard, bool mouse, bool gamepad);
    virtual void Stop();
    virtual std::string GetDeviceName(std::string_view id) const;

private:
    using EvdevDeviceMap = boost::unordered::concurrent_flat_map<std::string, evdev_device>;
    EvdevDeviceMap m_evdev_devices;
    void _init_scan_devices();
    void _init_poll(bool keyboard, bool mouse, bool gamepad);
    std::jthread m_device_scan_thread;
    std::jthread m_poll_thread;
};