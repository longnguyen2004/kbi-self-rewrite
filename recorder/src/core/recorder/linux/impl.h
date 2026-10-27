#pragma once

#include "../recorder_impl.h"
#include <boost/unordered/concurrent_flat_map.hpp>
#include <functional>
#include <libevdev/libevdev.h>
#include <memory>
#include <optional>
#include <poll.h>
#include <string>
#include <string_view>
#include <thread>

namespace Kbi {

// Frees an open libevdev device, including the file descriptor it owns.
struct EvdevCloser {
    void operator()(libevdev* device) const;
};

// Owning handle to an open libevdev device (R.20).
using EvdevPtr = std::unique_ptr<libevdev, EvdevCloser>;

struct InternalDevice {
    EvdevPtr eventDevice;
    std::string syspath;
    bool remove = false;
};

class RecorderLinuxLibevdev : public Recorder::Impl {
public:
    explicit RecorderLinuxLibevdev(std::shared_ptr<spdlog::logger> logger);
    ~RecorderLinuxLibevdev() override;
    void start(InputMask devices) override;
    void stop() override;
    std::string deviceName(std::string_view syspath) const override;
    std::optional<std::string> usbDeviceId(std::string_view id) const override;
    std::optional<UsbDeviceInfo> usbDeviceInfo(std::string_view id) const override;

private:
    using EvdevDeviceMap = boost::unordered::concurrent_flat_map<std::string, InternalDevice>;
    EvdevDeviceMap m_evdevDevices;
    void initScanDevices();
    void initPoll(InputMask devices);
    // Opens the newly discovered devices and refreshes the poll set; the
    // removed devices are closed here (kept out of initPoll, F.3).
    void collectDevices(InputMask devices,
        std::vector<std::reference_wrapper<InternalDevice>>& deviceList,
        std::vector<pollfd>& pollfds);
    std::jthread m_deviceScanThread;
    std::jthread m_pollThread;
};

} // namespace Kbi
