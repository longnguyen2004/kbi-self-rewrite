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
#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

using namespace boost::container;
using namespace boost::unordered;
using namespace std::literals;

#ifndef input_event_sec
#define input_event_sec time.tv_sec
#endif

#ifndef input_event_usec
#define input_event_usec time.tv_usec
#endif

libevdev* evdevOpen(const char* path)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK);
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

void evdevCloseP(libevdev** ptr)
{
    evdevClose(*ptr);
}

// The hotplug scan walks the inotify events of /dev/input/by-id, which
// needs a nested loop over the event stream.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void RecorderLinuxLibevdev::initScanDevices()
{
    auto defaultConstruct = [](std::string_view path) {
        return std::make_pair(std::string(path), InternalDevice { .syspath = std::string(path) });
    };
    glob_t globResult;
    if (glob("/dev/input/by-id/*event*", 0, nullptr, &globResult) == 0) {
        auto glPathc = globResult.gl_pathc;
        auto* glPathv = globResult.gl_pathv;
        m_evdevDevices.insert(
            boost::make_transform_iterator(glPathv, defaultConstruct),
            boost::make_transform_iterator(glPathv + glPathc, defaultConstruct));
        globfree(&globResult);
    }

    m_deviceScanThread = std::jthread([&](const std::stop_token& stop) {
        int notifyFd = inotify_init1(IN_NONBLOCK);
        if (notifyFd < 0) {
            m_logger->error("inotify_init1 failed");
            return;
        }
        if (inotify_add_watch(notifyFd, "/dev/input/by-id", IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM) < 0) {
            m_logger->error("inotify_add_watch failed for /dev/input/by-id");
            close(notifyFd);
            return;
        }
        pollfd pollStruct {
            .fd = notifyFd,
            .events = POLLIN
        };
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
                        m_logger->error("inotify read error: {}", strerror(errno));
                    break;
                }
                if (len == 0)
                    break;
                auto makeDevice = [](std::string_view path) {
                    return std::make_pair(std::string(path), InternalDevice { .syspath = std::string(path) });
                };
                static_vector<std::string, 100> added;
                static_vector<std::string, 100> removed;
                size_t offset = 0;
                while (std::cmp_less(offset, len)) {
                    auto* ev = reinterpret_cast<struct inotify_event*>(buf.data() + offset);
                    std::string_view name = ev->len ? std::string_view(ev->name, ev->len) : std::string_view {};
                    // Trim trailing NUL if present
                    if (!name.empty() && name.back() == '\0')
                        name.remove_suffix(1);
                    // Skip empty or hidden/temp names
                    if (!name.empty() && name[0] != '.' && name.contains("event")) {
                        std::string fullPath = std::format("/dev/input/by-id/{}", name);
                        if (ev->mask & IN_CREATE || ev->mask & IN_MOVED_TO)
                            added.emplace_back(std::move(fullPath));
                        else if (ev->mask & IN_DELETE || ev->mask & IN_MOVED_FROM)
                            removed.emplace_back(std::move(fullPath));
                    }
                    offset += sizeof(struct inotify_event) + ev->len;
                }
                for (auto& p : added)
                    m_evdevDevices.insert(makeDevice(p));
                m_evdevDevices.visit(removed.begin(), removed.end(),
                    [](EvdevDeviceMap::value_type& dev) {
                        dev.second.remove = true;
                    });
            }
        }
        close(notifyFd);
    });
}

// The poll loop handles the libevdev status codes and the input device
// filtering, which cannot be split up without changing its behaviour.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void RecorderLinuxLibevdev::initPoll(bool keyboard, bool mouse, bool gamepad)
{
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    m_pollThread = std::jthread([=, this](const std::stop_token& stop) {
        timespec ref;
        clock_gettime(CLOCK_MONOTONIC, &ref);
        std::uint64_t refUsec = ref.tv_sec * 1000000 + ref.tv_nsec / 1000;
        std::vector<InternalDevice*> devices;
        std::vector<pollfd> pollfds;
        while (!stop.stop_requested()) {
            devices.clear();
            pollfds.clear();
            // I'm not sure if this is safe (modify elements inside erase_if). Let's see
            m_evdevDevices.erase_if([&](EvdevDeviceMap::value_type& val) {
                auto& [path, dev] = val;
                if (dev.remove) {
                    m_logger->debug("Device {} removed", path);
                    evdevClose(dev.eventDevice);
                    return true;
                }
                // New device, try to open it
                if (!dev.eventDevice) {
                    __attribute__((cleanup(evdevCloseP))) libevdev* eventDevice = evdevOpen(path.data());
                    if (!eventDevice) {
                        m_logger->warn(
                            "Failed to open device {}. Are you running as root/in the 'input' user group?",
                            path);
                        return true;
                    }

                    // ignore virtual devices
                    if (!libevdev_get_phys(eventDevice))
                        return true;

                    // remove devices that don't send keys
                    if (!libevdev_has_event_type(eventDevice, EV_KEY))
                        return true;

                    // disable all events, and only enable the one we need
                    libevdev_disable_event_type(eventDevice, EV_REL);
                    libevdev_disable_event_type(eventDevice, EV_ABS);
                    libevdev_disable_event_type(eventDevice, EV_MSC);
                    libevdev_disable_event_type(eventDevice, EV_SW);
                    libevdev_disable_event_type(eventDevice, EV_LED);
                    libevdev_disable_event_type(eventDevice, EV_SND);
                    libevdev_disable_event_type(eventDevice, EV_REP);
                    libevdev_disable_event_type(eventDevice, EV_FF);
                    libevdev_disable_event_type(eventDevice, EV_PWR);
                    libevdev_disable_event_type(eventDevice, EV_FF_STATUS);
                    if (!keyboard) {
                        for (int i = 0; i < 256; ++i)
                            libevdev_disable_event_code(eventDevice, EV_KEY, i);
                    }
                    if (!mouse) {
                        for (int i = BTN_LEFT; i <= BTN_TASK; ++i)
                            libevdev_disable_event_code(eventDevice, EV_KEY, i);
                    }
                    if (!gamepad) {
                        // Disable common gamepad button codes
                        for (int i = BTN_GAMEPAD; i <= BTN_DPAD_RIGHT; ++i)
                            libevdev_disable_event_code(eventDevice, EV_KEY, i);
                        // Also disable trigger happy buttons
                        for (int i = BTN_TRIGGER_HAPPY1; i <= BTN_TRIGGER_HAPPY32; ++i)
                            libevdev_disable_event_code(eventDevice, EV_KEY, i);
                    }
                    libevdev_set_clock_id(eventDevice, CLOCK_MONOTONIC);
                    dev.eventDevice = eventDevice;
                    eventDevice = nullptr;
                    m_logger->debug("Opened device {}", path);
                }
                devices.push_back(&dev);
                pollfds.emplace_back(libevdev_get_fd(dev.eventDevice), POLLIN, 0);
                return false;
            });

            // poll all the devices
            int pollRes = poll(pollfds.data(), pollfds.size(), 1);
            if (pollRes == 0)
                continue;
            if (pollRes < 0)
                // an error happened, we might want to log this
                continue;

            // check which device has inputs
            for (int i = 0; i < pollfds.size(); ++i) {
                if (!(pollfds[i].revents & POLLIN))
                    continue;
                auto* device = devices[i]->eventDevice;
                auto& syspath = devices[i]->syspath;
                input_event ev;
                int rc;
                int flags = LIBEVDEV_READ_FLAG_NORMAL;
                std::uint16_t vid = libevdev_get_id_vendor(device);
                std::uint16_t pid = libevdev_get_id_product(device);
                while (true) {
                    rc = libevdev_next_event(device, flags, &ev);
                    if (rc == LIBEVDEV_READ_STATUS_SUCCESS) {
                        if (ev.type != EV_KEY || ev.value == 2)
                            continue;
                        onInput()(syspath, vid, pid, Input { .timestamp = (ev.input_event_sec * 1000000ULL + ev.input_event_usec - refUsec), .pressed = static_cast<bool>(ev.value), .code = evdevToKeycode(ev.code) });
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

RecorderLinuxLibevdev::RecorderLinuxLibevdev(std::shared_ptr<spdlog::logger> logger)
    : Recorder::Impl(std::move(logger))
{
    if (geteuid() != 0)
        m_logger->warn("The program is not running as root. You might not be able to capture inputs");
}

void RecorderLinuxLibevdev::start(bool keyboard, bool mouse, bool gamepad)
{
    initScanDevices();
    initPoll(keyboard, mouse, gamepad);
}

void RecorderLinuxLibevdev::stop()
{
    m_deviceScanThread.request_stop();
    m_pollThread.request_stop();
    m_deviceScanThread.join();
    m_pollThread.join();
    m_evdevDevices.visit_all([](EvdevDeviceMap::value_type& entry) {
        evdevClose(entry.second.eventDevice);
    });
    m_evdevDevices.clear();
}

std::string RecorderLinuxLibevdev::getDeviceName(std::string_view syspath) const
{
    return deviceNameFromPath(syspath);
}

std::optional<std::string> RecorderLinuxLibevdev::getUsbDeviceId(std::string_view id) const
{
    return findUsbDevice(id);
}

std::optional<UsbDeviceInfo> RecorderLinuxLibevdev::getUsbDeviceInfo(std::string_view id) const
{
    return usbDeviceInfoFromPath(id);
}
