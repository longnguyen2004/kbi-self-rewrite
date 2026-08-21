#include "impl.h"
#include "device_name.h"
#include <boost/container/static_vector.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <spdlog/spdlog.h>

#include <format>
#include <thread>
#include <string>
#include <string_view>
#include <utility>

#include <fcntl.h>
#include <glob.h>
#include <poll.h>
#include <unistd.h>
#include <sys/inotify.h>
#include "evdev_to_keycode.h"

using namespace boost::container;
using namespace boost::unordered;
using namespace std::literals;

#ifndef input_event_sec
#define input_event_sec time.tv_sec
#endif

#ifndef input_event_usec
#define input_event_usec time.tv_usec
#endif


bool is_keyboard(libevdev* device)
{
    return libevdev_has_event_code(device, EV_KEY, BTN_A);
}

bool is_mouse(libevdev* device)
{
    return libevdev_has_event_code(device, EV_KEY, BTN_LEFT);
}

bool is_gamepad(libevdev* device)
{
    // TODO: get a gamepad and implement this
    return false;
}

libevdev* evdev_open(const char* path)
{
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
        return nullptr;
    }
    libevdev* device;
    if (libevdev_new_from_fd(fd, &device) != 0)
    {
        close(fd);
        return nullptr;
    }
    return device;
}

void evdev_close(libevdev* device)
{
    if (!device)
        return;
    int fd = libevdev_get_fd(device);
    if (fd > 0)
        close(fd);
    libevdev_free(device);
}

void evdev_close_p(libevdev** ptr)
{
    evdev_close(*ptr);
}

void recorder_linux_libevdev::_init_scan_devices()
{
    auto default_construct = [](std::string_view path) {
        return std::make_pair(std::string(path), internal_device{
            .syspath = std::string(path)
        });
    };
    glob_t glob_result;
    if (glob("/dev/input/by-id/*event*", 0, nullptr, &glob_result) == 0)
    {
        auto gl_pathc = glob_result.gl_pathc;
        auto gl_pathv = glob_result.gl_pathv;
        m_evdev_devices.insert(
            boost::make_transform_iterator(gl_pathv, default_construct),
            boost::make_transform_iterator(gl_pathv + gl_pathc, default_construct)
        );
        globfree(&glob_result);
    }

    m_device_scan_thread = std::jthread([&](const std::stop_token& stop) {
        int notify_fd = inotify_init1(IN_NONBLOCK);
        if (notify_fd < 0) {
            m_logger->error("inotify_init1 failed");
            return;
        }
        if (inotify_add_watch(notify_fd, "/dev/input/by-id", IN_CREATE | IN_DELETE | IN_MOVED_TO | IN_MOVED_FROM) < 0) {
            m_logger->error("inotify_add_watch failed for /dev/input/by-id");
            close(notify_fd);
            return;
        }
        pollfd poll_struct{
            .fd = notify_fd,
            .events = POLLIN
        };
        constexpr size_t buf_size = 4096;
        std::array<char, buf_size> buf;
        while (!stop.stop_requested())
        {
            int result = poll(&poll_struct, 1, 100);
            if (result <= 0)
                continue;
            while (true)
            {
                ssize_t len = read(notify_fd, buf.data(), buf.size());
                if (len < 0)
                {
                    if (errno != EAGAIN)
                        m_logger->error("inotify read error: {}", strerror(errno));
                    break;
                }
                if (len == 0)
                    break;
                auto make_device = [](std::string_view path) {
                    return std::make_pair(std::string(path), internal_device{
                        .syspath = std::string(path)
                    });
                };
                static_vector<std::string, 100> added, removed;
                size_t offset = 0;
                while (offset < static_cast<size_t>(len))
                {
                    auto* ev = reinterpret_cast<struct inotify_event*>(buf.data() + offset);
                    std::string_view name = ev->len ? std::string_view(ev->name, ev->len) : std::string_view{};
                    // Trim trailing NUL if present
                    if (!name.empty() && name.back() == '\0')
                        name.remove_suffix(1);
                    // Skip empty or hidden/temp names
                    if (!name.empty() && name[0] != '.' && name.find("event") != std::string::npos)
                    {
                        std::string full_path = std::format("/dev/input/by-id/{}", name);
                        if (ev->mask & IN_CREATE || ev->mask & IN_MOVED_TO)
                            added.emplace_back(std::move(full_path));
                        else if (ev->mask & IN_DELETE || ev->mask & IN_MOVED_FROM)
                            removed.emplace_back(std::move(full_path));
                    }
                    offset += sizeof(struct inotify_event) + ev->len;
                }
                for (auto& p : added)
                    m_evdev_devices.insert(make_device(p));
                m_evdev_devices.visit(removed.begin(), removed.end(),
                    [](EvdevDeviceMap::value_type& dev) {
                        dev.second.remove = true;
                    }
                );
            }
        }
        close(notify_fd);
    });
}

void recorder_linux_libevdev::_init_poll(bool keyboard, bool mouse, bool gamepad)
{
    m_poll_thread = std::jthread([=, this](const std::stop_token& stop) {
        timespec ref;
        clock_gettime(CLOCK_MONOTONIC, &ref);
        std::uint64_t ref_usec = ref.tv_sec * 1000000 + ref.tv_nsec / 1000;
        std::vector<internal_device*> devices;
        std::vector<pollfd> pollfds;
        while (!stop.stop_requested())
        {
            devices.clear();
            pollfds.clear();
            // I'm not sure if this is safe (modify elements inside erase_if). Let's see
            m_evdev_devices.erase_if([&](EvdevDeviceMap::value_type& val) {
                auto& [path, dev] = val;
                if (dev.remove)
                {
                    m_logger->debug("Device {} removed", path);
                    evdev_close(dev.event_device);
                    return true;
                }
                // New device, try to open it
                if (!dev.event_device)
                {
                    __attribute__((cleanup(evdev_close_p))) libevdev* event_device = evdev_open(path.data());
                    if (!event_device)
                    {
                        m_logger->warn(
                            "Failed to open device {}. Are you running as root/in the 'input' user group?",
                            path
                        );
                        return true;
                    }

                    // ignore virtual devices
                    if (!libevdev_get_phys(event_device))
                        return true;

                    // remove devices that don't send keys
                    if (!libevdev_has_event_type(event_device, EV_KEY))
                        return true;

                    // disable all events, and only enable the one we need
                    libevdev_disable_event_type(event_device, EV_REL);
                    libevdev_disable_event_type(event_device, EV_ABS);
                    libevdev_disable_event_type(event_device, EV_MSC);
                    libevdev_disable_event_type(event_device, EV_SW);
                    libevdev_disable_event_type(event_device, EV_LED);
                    libevdev_disable_event_type(event_device, EV_SND);
                    libevdev_disable_event_type(event_device, EV_REP);
                    libevdev_disable_event_type(event_device, EV_FF);
                    libevdev_disable_event_type(event_device, EV_PWR);
                    libevdev_disable_event_type(event_device, EV_FF_STATUS);
                    if (!keyboard)
                    {
                        for (int i = 0; i < 256; ++i)
                            libevdev_disable_event_code(event_device, EV_KEY, i);
                    }
                    if (!mouse)
                    {
                        for (int i = BTN_LEFT; i <= BTN_TASK; ++i)
                            libevdev_disable_event_code(event_device, EV_KEY, i);
                    }
                    if (!gamepad)
                    {
                        // TODO: fill in gamepad codes
                    }
                    libevdev_set_clock_id(event_device, CLOCK_MONOTONIC);
                    dev.event_device = event_device;
                    event_device = nullptr;
                    m_logger->debug("Opened device {}", path);
                }
                devices.push_back(&dev);
                pollfds.emplace_back(libevdev_get_fd(dev.event_device), POLLIN, 0);
                return false;
            });

            // poll all the devices
            int poll_res = poll(pollfds.data(), pollfds.size(), 1);
            if (poll_res == 0)
                continue;
            if (poll_res < 0)
                // an error happened, we might want to log this
                continue;

            // check which device has inputs
            for (int i = 0; i < pollfds.size(); ++i)
            {
                if (!(pollfds[i].revents & POLLIN))
                    continue;
                auto device = devices[i]->event_device;
                auto& syspath = devices[i]->syspath;
                input_event ev;
                int rc, flags = LIBEVDEV_READ_FLAG_NORMAL;
                std::uint16_t vid = libevdev_get_id_vendor(device);
                std::uint16_t pid = libevdev_get_id_product(device);
                while (true)
                {
                    rc = libevdev_next_event(device, flags, &ev);
                    if (rc == LIBEVDEV_READ_STATUS_SUCCESS)
                    {
                        if (ev.type != EV_KEY || ev.value == 2)
                            continue;
                        OnInput()(syspath, vid, pid, Input{
                            .Timestamp = (ev.input_event_sec * 1000000ULL + ev.input_event_usec - ref_usec),
                            .Pressed = static_cast<bool>(ev.value),
                            .Code = evdev_to_keycode(ev.code)
                        });
                        continue;
                    }
                    else if (rc == LIBEVDEV_READ_STATUS_SYNC)
                    {
                        flags = LIBEVDEV_READ_FLAG_SYNC;
                        continue;
                    }
                    else if (rc == -EAGAIN)
                    {
                        flags = LIBEVDEV_READ_FLAG_NORMAL;
                    }
                    break;
                }
            }
        }
    });
}

recorder_linux_libevdev::recorder_linux_libevdev(std::shared_ptr<spdlog::logger> logger):
    Recorder::Impl(logger)
{
    if (geteuid() != 0)
        m_logger->warn("The program is not running as root. You might not be able to capture inputs");
}

void recorder_linux_libevdev::Start(bool keyboard, bool mouse, bool gamepad)
{
    _init_scan_devices();
    _init_poll(keyboard, mouse, gamepad);
}

void recorder_linux_libevdev::Stop()
{
    m_device_scan_thread.request_stop();
    m_poll_thread.request_stop();
    m_device_scan_thread.join();
    m_poll_thread.join();
    m_evdev_devices.visit_all([](EvdevDeviceMap::value_type& entry) {
        evdev_close(entry.second.event_device);
    });
    m_evdev_devices.clear();
}

std::string recorder_linux_libevdev::GetDeviceName(std::string_view syspath) const
{
    return device_name_from_path(syspath);
} 

std::optional<std::string> recorder_linux_libevdev::GetUsbDeviceId(std::string_view id) const
{
    return find_usb_device(id);
}

std::optional<UsbDeviceInfo> recorder_linux_libevdev::GetUsbDeviceInfo(std::string_view id) const
{
    return get_usb_device_info(id);
}
