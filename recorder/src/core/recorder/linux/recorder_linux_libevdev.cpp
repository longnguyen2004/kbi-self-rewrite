#include "device_name.h"
#include "impl.h"
#include <boost/container/static_vector.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <spdlog/spdlog.h>

#include <format>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "evdev_to_keycode.h"
#include <fcntl.h>
#include <glob.h>
#include <libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

namespace Kbi {

using namespace boost::container;
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

// The hotplug scan walks the inotify events of /dev/input/by-id, which
// needs a nested loop over the event stream.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void RecorderLinuxLibevdev::initScanDevices()
{
    auto defaultConstruct = [](std::string_view path) {
        return std::make_pair(std::string(path), InternalDevice { .syspath = std::string(path) });
    };
    glob_t globResult {};
    if (glob("/dev/input/by-id/*event*", 0, nullptr, &globResult) == 0) {
        const auto glPathc = globResult.gl_pathc;
        auto* const glPathv = globResult.gl_pathv;
        m_evdevDevices.insert(boost::make_transform_iterator(glPathv, defaultConstruct),
            boost::make_transform_iterator(glPathv + glPathc, defaultConstruct));
    }
    // glob may have allocated storage even when it fails, always release it
    // (P.8).
    globfree(&globResult);

    m_deviceScanThread = std::jthread([&](const std::stop_token& stop) {
        int notifyFd = inotify_init1(IN_NONBLOCK);
        if (notifyFd < 0) {
            logger()->error("inotify_init1 failed");
            return;
        }
        if (inotify_add_watch(
                notifyFd, "/dev/input/by-id", IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM)
            < 0) {
            logger()->error("inotify_add_watch failed for /dev/input/by-id");
            close(notifyFd);
            return;
        }
        pollfd pollStruct { .fd = notifyFd, .events = POLLIN };
        constexpr size_t bufSize = 4096;
        std::array<char, bufSize> buf;
        while (!stop.stop_requested()) {
            int result = poll(&pollStruct, 1, 100);
            if (result <= 0)
                continue;
            while (true) {
                ssize_t len = read(notifyFd, buf.data(), buf.size());
                if (len < 0) {
                    if (errno != EAGAIN)
                        logger()->error("inotify read error: {}", strerror(errno));
                    break;
                }
                if (len == 0)
                    break;
                auto makeDevice = [](std::string_view path) {
                    return std::make_pair(
                        std::string(path), InternalDevice { .syspath = std::string(path) });
                };
                static_vector<std::string, 100> added;
                static_vector<std::string, 100> removed;
                size_t offset = 0;
                while (std::cmp_less(offset, len)) {
                    auto* ev = reinterpret_cast<struct inotify_event*>(buf.data() + offset);
                    std::string_view name
                        = ev->len ? std::string_view(ev->name, ev->len) : std::string_view {};
                    // Trim trailing NUL if present
                    if (!name.empty() && name.back() == '\0')
                        name.remove_suffix(1);
                    // Skip empty or hidden/temp names
                    if (!name.empty() && name[0] != '.' && name.contains("event")) {
                        std::string fullPath = std::format("/dev/input/by-id/{}", name);
                        if ((ev->mask & IN_CREATE) || (ev->mask & IN_MOVED_TO))
                            added.emplace_back(std::move(fullPath));
                        else if ((ev->mask & IN_DELETE) || (ev->mask & IN_MOVED_FROM))
                            removed.emplace_back(std::move(fullPath));
                    }
                    offset += sizeof(struct inotify_event) + ev->len;
                }
                for (const auto& p : added)
                    m_evdevDevices.insert(makeDevice(p));
                m_evdevDevices.visit(removed.begin(), removed.end(),
                    [](EvdevDeviceMap::value_type& dev) { dev.second.remove = true; });
            }
        }
        close(notifyFd);
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
