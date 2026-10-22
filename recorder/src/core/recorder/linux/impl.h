#include "../recorder_impl.h"
#include <boost/unordered/concurrent_flat_map.hpp>
#include <libevdev/libevdev.h>
#include <thread>

struct InternalDevice {
    libevdev* eventDevice = nullptr;
    std::string syspath;
    bool remove = false;
};

class RecorderLinuxLibevdev : public Recorder::Impl {
public:
    explicit RecorderLinuxLibevdev(std::shared_ptr<spdlog::logger> logger);
    void start(bool keyboard, bool mouse, bool gamepad) override;
    void stop() override;
    std::string getDeviceName(std::string_view syspath) const override;
    std::optional<std::string> getUsbDeviceId(std::string_view id) const override;
    std::optional<UsbDeviceInfo> getUsbDeviceInfo(std::string_view id) const override;

private:
    using EvdevDeviceMap = boost::unordered::concurrent_flat_map<std::string, InternalDevice>;
    EvdevDeviceMap m_evdevDevices;
    void initScanDevices();
    void initPoll(bool keyboard, bool mouse, bool gamepad);
    std::jthread m_deviceScanThread;
    std::jthread m_pollThread;
};