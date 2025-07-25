#include "../recorder_impl.h"
#include <thread>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <libevdev/libevdev.h>

struct internal_device
{
    libevdev* event_device = nullptr;
    std::string syspath;
    bool remove = false;
};

class recorder_linux_libevdev: public Recorder::Impl
{
public:
    recorder_linux_libevdev(std::shared_ptr<spdlog::logger> logger);
    virtual void Start(bool kseyboard, bool mouse, bool gamepad);
    virtual void Stop();
    virtual std::string GetDeviceName(std::string_view id) const;
    virtual std::optional<std::string> GetUsbDeviceId(std::string_view id) const;
    virtual std::optional<UsbDeviceInfo> GetUsbDeviceInfo(std::string_view id) const;

private:
    using EvdevDeviceMap = boost::unordered::concurrent_flat_map<std::string, internal_device>;
    EvdevDeviceMap m_evdev_devices;
    void _init_scan_devices();
    void _init_poll(bool keyboard, bool mouse, bool gamepad);
    std::jthread m_device_scan_thread;
    std::jthread m_poll_thread;
};