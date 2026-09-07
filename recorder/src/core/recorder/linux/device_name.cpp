#include "device_name.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <tuple>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <linux/limits.h>

namespace fs = std::filesystem;

static std::optional<std::string> read_sysfs_attr(std::string_view path, const char* attr)
{
    auto file_path = (fs::path(path) / attr).string();
    std::ifstream file(file_path);
    if (!file)
        return std::nullopt;
    std::string value;
    std::getline(file, value);
    if (value.empty())
        return std::nullopt;
    return value;
}

// Read a sysfs symlink (subsystem, driver) and return its basename.
// e.g. readlink("/sys/.../subsystem") -> "../../../../bus/hid" -> "hid"
static std::string read_sysfs_link_basename(std::string_view path, const char* attr)
{
    auto file_path = (fs::path(path) / attr).string();
    char buf[PATH_MAX];
    ssize_t len = readlink(file_path.c_str(), buf, sizeof(buf) - 1);
    if (len < 0)
        return {};
    buf[len] = '\0';
    return fs::path(std::string_view(buf, len)).filename().string();
}

// Convert a /dev/input/... path (or /dev/input/by-id/...) to its
// corresponding sysfs path via /sys/class/input/<basename>.
// If path is already a sysfs path, just canonicalize it.
static bool is_sysfs_path(std::string_view path)
{
    fs::path p(path);
    auto it = p.begin();
    if (it == p.end() || *it != "/")
        return false;
    ++it;
    return it != p.end() && *it == "sys";
}

static std::optional<std::string> dev_to_syspath(std::string_view path)
{
    if (is_sysfs_path(path))
    {
        char resolved[PATH_MAX];
        if (!realpath(std::string(path).data(), resolved))
            return std::nullopt;
        return std::string(resolved);
    }

    char dev_real[PATH_MAX];
    if (!realpath(std::string(path).data(), dev_real))
        return std::nullopt;

    std::string basename = fs::path(dev_real).filename().string();
    // All recorder devices are evdev events under /sys/class/input
    auto class_path = (fs::path("/sys/class/input") / basename).string();
    char sys_real[PATH_MAX];
    if (realpath(class_path.c_str(), sys_real))
        return std::string(sys_real);

    // Fallback: maybe the path was already a sysfs-like path that
    // just wasn't under /sys/class/input (e.g. hidraw). Try direct
    // realpath as last resort.
    return std::nullopt;
}

// Get parent device, mimicking sd_device_get_parent(): follow the
// "device" symlink if present, otherwise use filesystem parent.
static std::string get_parent_device(const std::string& cur)
{
    auto device_link = (fs::path(cur) / "device").string();
    char resolved[PATH_MAX];
    if (realpath(device_link.c_str(), resolved))
        return std::string(resolved);
    return fs::path(cur).parent_path().string();
}

std::tuple<const char*, const char*> subsys_driver_chain[] = {
    { "input", "" },
    { "input", "" },
    { "hid", "hid-generic" },
    { "usb", "usbhid" },
};

std::optional<std::string> find_usb_device(std::string_view path)
{
    auto syspath = dev_to_syspath(path);
    if (!syspath)
        return {};
    std::string current = *syspath;

    for (auto& [subsys, driver] : subsys_driver_chain)
    {
        auto device_subsys = read_sysfs_link_basename(current, "subsystem");
        if (device_subsys.empty())
            return {};
        auto device_driver = read_sysfs_link_basename(current, "driver");

        if (std::strcmp(subsys, device_subsys.c_str()) != 0)
            return {};
        if (std::strcmp(driver, device_driver.c_str()) != 0)
            return {};

        auto parent = get_parent_device(current);
        if (parent == current || parent.empty())
            return {};
        current = std::move(parent);
    }

    return current;
}

std::optional<UsbDeviceInfo> get_usb_device_info(std::string_view path)
{
    UsbDeviceInfo devInfo;

    auto vid_str = read_sysfs_attr(path, "idVendor");
    if (!vid_str)
        return {};
    devInfo.VID = std::strtoul(vid_str->c_str(), nullptr, 16);

    auto pid_str = read_sysfs_attr(path, "idProduct");
    if (!pid_str)
        return {};
    devInfo.PID = std::strtoul(pid_str->c_str(), nullptr, 16);

    auto speed_str = read_sysfs_attr(path, "speed");
    if (!speed_str)
        return {};
    if (*speed_str == "1.5")
        devInfo.Speed = UsbDeviceSpeed::LOW_SPEED;
    else if (*speed_str == "12")
        devInfo.Speed = UsbDeviceSpeed::FULL_SPEED;
    else if (*speed_str == "480")
        devInfo.Speed = UsbDeviceSpeed::HIGH_SPEED;
    else
        devInfo.Speed = UsbDeviceSpeed::SUPERSPEED;

    auto descriptor_path = (fs::path(path) / "descriptors").string();
    auto fd = open(descriptor_path.c_str(), O_RDONLY);
    if (fd >= 0)
    {
        std::vector<unsigned char> descriptors;
        unsigned char buf[1024];
        int ret;
        while (true)
        {
            ret = read(fd, buf, sizeof(buf));
            if (ret < 0)
            {
                break;
            }
            else if (ret == 0)
            {
                devInfo.Descriptors = descriptors;
                break;
            }
            descriptors.insert(descriptors.end(), buf, buf + ret);
        }
        close(fd);
    }
    return devInfo;
}

std::string device_name_from_path(std::string_view path)
{
    auto syspath = dev_to_syspath(path);
    if (!syspath)
        return "Unknown";

    std::string current = *syspath;
    for (int i = 0; i < 10; ++i)
    {
        auto name = read_sysfs_attr(current, "name");
        if (name)
            return *name;

        auto parent = get_parent_device(current);
        if (parent == current || parent.empty())
            break;
        current = std::move(parent);
    }
    return "Unknown";
}
