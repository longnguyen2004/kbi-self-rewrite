#include "device_name.h"
#include "impl.h"
#include <spdlog/spdlog.h>

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "evdev_to_keycode.h"
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <systemd/sd-device.h>
#include <unistd.h>

namespace Kbi {

using namespace boost::unordered;

// Compatibility with older kernels: struct input_event used to carry its
// timestamps in a `struct timeval time`, newer ones have dedicated
// `input_event_sec`/`input_event_usec` members. The kernel header defines
// these lower-case macros itself when needed (see linux/input.h), so the
// names must stay lower-case and match the member names (NL.9 exception).
#ifndef input_event_sec
#define input_event_sec time.tv_sec
#endif

#ifndef input_event_usec
#define input_event_usec time.tv_usec
#endif

namespace {

// Microseconds per second; event timestamps are in microseconds.
constexpr std::uint64_t kMicrosPerSecond = 1'000'000ULL;

libevdev* evdevOpen(const std::string& path)
{
    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return nullptr;
    }
    libevdev* device;
    if (libevdev_new_from_fd(fd, &device) != 0) {
        close(fd);
        return nullptr;
    }
    return device;
}

void evdevClose(libevdev* device)
{
    if (device == nullptr)
        return;
    int fd = libevdev_get_fd(device);
    if (fd > 0)
        close(fd);
    libevdev_free(device);
}

} // namespace

void EvdevCloser::operator()(libevdev* device) const { evdevClose(device); }

struct SdDeviceCloser {
    void operator()(sd_device* device) const { sd_device_unref(device); }
};
struct SdEnumeratorCloser {
    void operator()(sd_device_enumerator* enumerator) const { sd_device_enumerator_unref(enumerator); }
};
struct SdMonitorCloser {
    void operator()(sd_device_monitor* monitor) const { sd_device_monitor_unref(monitor); }
};
using SdDevicePtr = std::unique_ptr<sd_device, SdDeviceCloser>;
using SdEnumeratorPtr = std::unique_ptr<sd_device_enumerator, SdEnumeratorCloser>;
using SdMonitorPtr = std::unique_ptr<sd_device_monitor, SdMonitorCloser>;

// An input event device satellite node (eventXX under /dev/input).
bool isEventDevice(sd_device* device)
{
    const char* sysname = nullptr;
    if (sd_device_get_sysname(device, &sysname) < 0 || sysname == nullptr)
        return false;
    return std::string_view(sysname).contains("event");
}

// Unique, durable device ID used in recordings: prefer the udev-stable
// /dev/input/by-id symlink (stable across replugging/reboot, unlike the
// runtime-numbered /dev/input/eventXX devnode), falling back to the devnode
// for devices that have no by-id link (e.g. non-USB i8042 devices).
std::optional<std::string> inputDeviceId(sd_device* device)
{
    for (const char* link = sd_device_get_devlink_first(device); link != nullptr;
         link = sd_device_get_devlink_next(device)) {
        const std::string_view linkView(link);
        if (linkView.starts_with("/dev/input/by-id/") && linkView.contains("event"))
            return std::string(linkView);
    }
    const char* devnode = nullptr;
    if (sd_device_get_devname(device, &devnode) < 0 || devnode == nullptr)
        return std::nullopt;
    return std::string(devnode);
}

// The hotplug scan lists the already-present input devices with an
// sd_device_enumerator, then watches the udev monitor for additions and
// removals. The monitor needs several setup steps, hence the linear bodies.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void RecorderLinuxLibevdev::initScanDevices()
{
    auto makeDevice = [](std::string_view path) {
        return std::make_pair(std::string(path), InternalDevice { .syspath = std::string(path) });
    };

    // Initial scan: devices already present when the recorder starts.
    sd_device_enumerator* enumeratorRaw = nullptr;
    if (sd_device_enumerator_new(&enumeratorRaw) < 0) {
        logger()->error("sd_device_enumerator_new failed");
        return;
    }
    SdEnumeratorPtr enumerator { enumeratorRaw };
    sd_device_enumerator_add_match_subsystem(enumeratorRaw, "input", true);
    // The enumerator skips devices without a udev database entry by default;
    // enumerate uninitialized devices too, or nothing may be found.
    sd_device_enumerator_allow_uninitialized(enumeratorRaw);
    // get_device_next() only advances the scan; iteration must start with
    // get_device_first().
    for (sd_device* device = sd_device_enumerator_get_device_first(enumeratorRaw); device != nullptr;
         device = sd_device_enumerator_get_device_next(enumeratorRaw)) {
        // The reference is owned by the enumerator, don't unref it.
        if (!isEventDevice(device))
            continue;
        const auto id = inputDeviceId(device);
        if (!id)
            continue;
        m_evdevDevices.insert(makeDevice(*id));
    }

    // Hotplug detection: receive devices from the udev monitor instead of
    // parsing raw inotify events of /dev/input/by-id.
    m_deviceScanThread = std::jthread([&](const std::stop_token& stop) {
        sd_device_monitor* monitorRaw = nullptr;
        if (sd_device_monitor_new(&monitorRaw) < 0) {
            logger()->error("sd_device_monitor_new failed");
            return;
        }
        SdMonitorPtr monitor { monitorRaw };
        if (sd_device_monitor_filter_add_match_subsystem_devtype(monitorRaw, "input", nullptr) < 0
            || sd_device_monitor_attach_event(monitorRaw, nullptr) < 0
            || sd_device_monitor_start(monitorRaw, nullptr, nullptr) < 0) {
            logger()->error("failed to start the udev device monitor");
            return;
        }
        const int monitorFd = sd_device_monitor_get_fd(monitorRaw);
        if (monitorFd < 0) {
            logger()->error("sd_device_monitor_get_fd failed");
            return;
        }
        while (!stop.stop_requested()) {
            pollfd pollStruct { .fd = monitorFd, .events = POLLIN };
            const int pollResult = poll(&pollStruct, 1, 100);
            if (pollResult <= 0)
                continue;
            while (true) {
                sd_device* received = nullptr;
                const int receiveResult = sd_device_monitor_receive(monitorRaw, &received);
                if (receiveResult == 0)
                    break;
                if (receiveResult < 0) {
                    logger()->error("sd_device_monitor_receive error: {}", strerror(-receiveResult));
                    break;
                }
                SdDevicePtr device { received };
                if (!isEventDevice(received))
                    continue;
                auto id = inputDeviceId(received);
                if (!id) {
                    // Devices synthesized from the udev database on removal
                    // normally keep their devnode/links; recover the identity
                    // from the sysname as a last resort.
                    const char* sysname = nullptr;
                    if (sd_device_get_sysname(received, &sysname) < 0 || sysname == nullptr)
                        continue;
                    id = std::format("/dev/input/{}", sysname);
                }
                sd_device_action_t action = _SD_DEVICE_ACTION_INVALID;
                if (sd_device_get_action(received, &action) < 0)
                    continue;
                if (action == SD_DEVICE_ADD) {
                    m_evdevDevices.insert(makeDevice(*id));
                } else if (action == SD_DEVICE_REMOVE) {
                    const std::string idString = *id;
                    m_evdevDevices.visit_all([&](EvdevDeviceMap::value_type& entry) {
                        if (entry.first == idString)
                            entry.second.remove = true;
                    });
                }
            }
        }
    });
}

// The poll loop handles the libevdev status codes and the input device
// filtering, which cannot be split up without changing its behaviour.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void RecorderLinuxLibevdev::initPoll(InputMask devices)
{
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    m_pollThread = std::jthread([this, devices](const std::stop_token& stop) {
        timespec ref;
        clock_gettime(CLOCK_MONOTONIC, &ref);
        const std::uint64_t refUsec = static_cast<std::uint64_t>(ref.tv_sec) * kMicrosPerSecond
            + static_cast<std::uint64_t>(ref.tv_nsec) / 1000;
        std::vector<std::reference_wrapper<InternalDevice>> deviceList;
        std::vector<pollfd> pollfds;
        while (!stop.stop_requested()) {
            deviceList.clear();
            pollfds.clear();
            collectDevices(devices, deviceList, pollfds);

            // poll all the devices
            int pollRes = poll(pollfds.data(), pollfds.size(), 1);
            if (pollRes == 0)
                continue;
            if (pollRes < 0) {
                // an error happened
                if (errno != EINTR)
                    logger()->error("evdev poll error: {}", strerror(errno));
                continue;
            }

            // check which device has inputs
            for (int i = 0; i < pollfds.size(); ++i) {
                if (!(pollfds[i].revents & POLLIN))
                    continue;
                InternalDevice& internalDevice = deviceList[i].get();
                auto* device = internalDevice.eventDevice.get();
                const auto& syspath = internalDevice.syspath;
                input_event ev;
                int rc = 0;
                int flags = LIBEVDEV_READ_FLAG_NORMAL;
                const VidPid ids
                    = { .vid = static_cast<std::uint16_t>(libevdev_get_id_vendor(device)),
                          .pid = static_cast<std::uint16_t>(libevdev_get_id_product(device)) };
                while (true) {
                    rc = libevdev_next_event(device, flags, &ev);
                    if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
                        if (ev.type != EV_KEY || ev.value == 2)
                            continue;
                        onInput()(syspath, ids,
                            Input { .timestamp
                                = static_cast<std::uint64_t>(ev.input_event_sec) * kMicrosPerSecond
                                    + static_cast<std::uint64_t>(ev.input_event_usec) - refUsec,
                                .pressed = static_cast<bool>(ev.value),
                                .code = evdevToKeycode(ev.code) });
                        continue;
                    }
                    if (rc == LIBEVDEV_READ_STATUS_SYNC) {
                        flags = LIBEVDEV_READ_FLAG_SYNC;
                        continue;
                    }
                    if (rc == -EAGAIN)
                        flags = LIBEVDEV_READ_FLAG_NORMAL;
                    break;
                }
            }
        }
    });
}

// Opens the newly discovered devices, disables the event codes of the device
// classes that are not recorded, and collects the open devices into the poll
// set. Devices flagged for removal are closed here.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void RecorderLinuxLibevdev::collectDevices(InputMask devices,
    std::vector<std::reference_wrapper<InternalDevice>>& deviceList, std::vector<pollfd>& pollfds)
{
    // I'm not sure if this is safe (modify elements inside erase_if). Let's see
    m_evdevDevices.erase_if([&](EvdevDeviceMap::value_type& val) {
        auto& [path, dev] = val;
        if (dev.remove) {
            logger()->debug("Device {} removed", path);
            // The device is closed by the InternalDevice destructor
            // when the entry is erased (R.20).
            return true;
        }
        // New device, try to open it
        if (!dev.eventDevice) {
            // The RAII owner closes the device on every early-return
            // path; ownership is moved into the map entry on success
            // (P.2, CPL.1).
            EvdevPtr eventDevice(evdevOpen(path));
            if (!eventDevice) {
                logger()->warn("Failed to open device {}. Are you running as root/in the "
                               "'input' user group?",
                    path);
                return true;
            }

            // ignore virtual devices
            if (!libevdev_get_phys(eventDevice.get()))
                return true;

            // remove devices that don't send keys
            if (!libevdev_has_event_type(eventDevice.get(), EV_KEY))
                return true;

            // disable all events, and only enable the one we need
            libevdev_disable_event_type(eventDevice.get(), EV_REL);
            libevdev_disable_event_type(eventDevice.get(), EV_ABS);
            libevdev_disable_event_type(eventDevice.get(), EV_MSC);
            libevdev_disable_event_type(eventDevice.get(), EV_SW);
            libevdev_disable_event_type(eventDevice.get(), EV_LED);
            libevdev_disable_event_type(eventDevice.get(), EV_SND);
            libevdev_disable_event_type(eventDevice.get(), EV_REP);
            libevdev_disable_event_type(eventDevice.get(), EV_FF);
            libevdev_disable_event_type(eventDevice.get(), EV_PWR);
            libevdev_disable_event_type(eventDevice.get(), EV_FF_STATUS);
            if (!has(devices, InputMask::Keyboard)) {
                for (int i = 0; i < 256; ++i)
                    libevdev_disable_event_code(eventDevice.get(), EV_KEY, i);
            }
            if (!has(devices, InputMask::Mouse)) {
                for (int i = BTN_LEFT; i <= BTN_TASK; ++i)
                    libevdev_disable_event_code(eventDevice.get(), EV_KEY, i);
            }
            if (!has(devices, InputMask::Gamepad)) {
                // Disable common gamepad button codes
                for (int i = BTN_GAMEPAD; i <= BTN_DPAD_RIGHT; ++i)
                    libevdev_disable_event_code(eventDevice.get(), EV_KEY, i);
                // Also disable trigger happy buttons
                for (int i = BTN_TRIGGER_HAPPY1; i <= BTN_TRIGGER_HAPPY32; ++i)
                    libevdev_disable_event_code(eventDevice.get(), EV_KEY, i);
            }
            libevdev_set_clock_id(eventDevice.get(), CLOCK_MONOTONIC);
            dev.eventDevice = std::move(eventDevice);
            logger()->debug("Opened device {}", path);
        }
        deviceList.emplace_back(dev);
        pollfds.emplace_back(libevdev_get_fd(dev.eventDevice.get()), POLLIN, 0);
        return false;
    });
}

RecorderLinuxLibevdev::RecorderLinuxLibevdev(std::shared_ptr<spdlog::logger> logger)
    : Recorder::Impl(std::move(logger))
{
    if (geteuid() != 0)
        Impl::logger()->warn(
            "The program is not running as root. You might not be able to capture inputs");
}

RecorderLinuxLibevdev::~RecorderLinuxLibevdev()
{
    // Stop the threads and close the devices even if the recorder is
    // destroyed without an explicit stop() (R.1); stop() is idempotent.
    stop();
}

void RecorderLinuxLibevdev::start(InputMask devices)
{
    initScanDevices();
    initPoll(devices);
}

void RecorderLinuxLibevdev::stop()
{
    m_deviceScanThread.request_stop();
    m_pollThread.request_stop();
    if (m_deviceScanThread.joinable())
        m_deviceScanThread.join();
    if (m_pollThread.joinable())
        m_pollThread.join();
    // The devices are closed by the InternalDevice destructors when the map
    // is cleared (R.20).
    m_evdevDevices.clear();
}

std::string RecorderLinuxLibevdev::deviceName(std::string_view syspath) const
{
    return deviceNameFromPath(syspath);
}

std::optional<std::string> RecorderLinuxLibevdev::usbDeviceId(std::string_view id) const
{
    return findUsbDevice(id);
}

std::optional<UsbDeviceInfo> RecorderLinuxLibevdev::usbDeviceInfo(std::string_view id) const
{
    return usbDeviceInfoFromPath(id);
}

} // namespace Kbi
