#include "device_name.h"
#include <array>
#include <cstdio>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <linux/limits.h>
#include <tuple>
#include <unistd.h>
#include <vector>

namespace Kbi {

namespace fs = std::filesystem;

namespace {

std::optional<std::string> readSysfsAttr(std::string_view path, std::string_view attr)
{
    const auto filePath = (fs::path(path) / attr).string();
    std::ifstream file(filePath);
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
std::string readSysfsLinkBasename(std::string_view path, std::string_view attr)
{
    const auto filePath = (fs::path(path) / attr).string();
    std::array<char, PATH_MAX> buf {};
    const ssize_t len = readlink(filePath.c_str(), buf.data(), buf.size() - 1);
    if (len < 0)
        return {};
    buf[len] = '\0';
    return fs::path(std::string_view(buf.data(), len)).filename().string();
}

// Convert a /dev/input/... path (or /dev/input/by-id/...) to its
// corresponding sysfs path via /sys/class/input/<basename>.
// If path is already a sysfs path, just canonicalize it.
bool isSysfsPath(std::string_view path)
{
    fs::path p(path);
    auto it = p.begin();
    if (it == p.end() || *it != "/")
        return false;
    ++it;
    return it != p.end() && *it == "sys";
}

std::optional<std::string> devToSyspath(std::string_view path)
{
    if (isSysfsPath(path)) {
        std::array<char, PATH_MAX> resolved {};
        if (realpath(std::string(path).data(), resolved.data()) == nullptr)
            return std::nullopt;
        return std::string(resolved.data());
    }

    std::array<char, PATH_MAX> devReal {};
    if (realpath(std::string(path).data(), devReal.data()) == nullptr)
        return std::nullopt;

    const std::string basename = fs::path(devReal.data()).filename().string();
    // All recorder devices are evdev events under /sys/class/input
    const auto classPath = (fs::path("/sys/class/input") / basename).string();
    std::array<char, PATH_MAX> sysReal {};
    if (realpath(classPath.c_str(), sysReal.data()) != nullptr)
        return std::string(sysReal.data());

    // Fallback: maybe the path was already a sysfs-like path that
    // just wasn't under /sys/class/input (e.g. hidraw). Try direct
    // realpath as last resort.
    return std::nullopt;
}

// Get parent device, mimicking sd_device_get_parent(): follow the
// "device" symlink if present, otherwise use filesystem parent.
std::string getParentDevice(const std::string& cur)
{
    const auto deviceLink = (fs::path(cur) / "device").string();
    std::array<char, PATH_MAX> resolved {};
    if (realpath(deviceLink.c_str(), resolved.data()) != nullptr)
        return { resolved.data() };
    return fs::path(cur).parent_path().string();
}

struct SubsysDriver {
    std::string_view subsys;
    std::string_view driver;
};

constexpr auto subsysDriverChain = std::to_array<SubsysDriver>({
    { .subsys = "input", .driver = "" },
    { .subsys = "input", .driver = "" },
    { .subsys = "hid", .driver = "hid-generic" },
    { .subsys = "usb", .driver = "usbhid" },
});

} // namespace

std::optional<std::string> findUsbDevice(std::string_view path)
{
    const auto syspath = devToSyspath(path);
    if (!syspath)
        return {};
    std::string current = *syspath;

    for (const auto& [subsys, driver] : subsysDriverChain) {
        auto deviceSubsys = readSysfsLinkBasename(current, "subsystem");
        if (deviceSubsys.empty())
            return {};
        auto deviceDriver = readSysfsLinkBasename(current, "driver");

        if (subsys != deviceSubsys)
            return {};
        if (driver != deviceDriver)
            return {};

        auto parent = getParentDevice(current);
        if (parent == current || parent.empty())
            return {};
        current = std::move(parent);
    }

    return current;
}

std::optional<UsbDeviceInfo> usbDeviceInfoFromPath(std::string_view path)
{
    UsbDeviceInfo devInfo;

    const auto vidStr = readSysfsAttr(path, "idVendor");
    if (!vidStr)
        return {};
    const unsigned long vidValue = std::strtoul(vidStr->c_str(), nullptr, 16);
    if (vidValue > 0xFFFF)
        return {};
    devInfo.vid = static_cast<std::uint16_t>(vidValue);

    const auto pidStr = readSysfsAttr(path, "idProduct");
    if (!pidStr)
        return {};
    const unsigned long pidValue = std::strtoul(pidStr->c_str(), nullptr, 16);
    if (pidValue > 0xFFFF)
        return {};
    devInfo.pid = static_cast<std::uint16_t>(pidValue);

    const auto speedStr = readSysfsAttr(path, "speed");
    if (!speedStr)
        return {};
    if (*speedStr == "1.5")
        devInfo.speed = UsbDeviceSpeed::LowSpeed;
    else if (*speedStr == "12")
        devInfo.speed = UsbDeviceSpeed::FullSpeed;
    else if (*speedStr == "480")
        devInfo.speed = UsbDeviceSpeed::HighSpeed;
    else
        devInfo.speed = UsbDeviceSpeed::SuperSpeed;

    const auto descriptorPath = (fs::path(path) / "descriptors").string();
    const auto fd = open(descriptorPath.c_str(), O_RDONLY);
    if (fd >= 0) {
        std::vector<unsigned char> descriptors;
        std::array<unsigned char, 1024> buf {};
        int ret;
        while (true) {
            ret = read(fd, buf.data(), buf.size());
            if (ret < 0) {
                break;
            }
            if (ret == 0) {
                devInfo.descriptors = std::move(descriptors);
                break;
            }
            descriptors.insert(descriptors.end(), buf.begin(), buf.begin() + ret);
        }
        close(fd);
    }
    return devInfo;
}

std::string deviceNameFromPath(std::string_view path)
{
    const auto syspath = devToSyspath(path);
    if (!syspath)
        return "Unknown";

    std::string current = *syspath;
    for (int i = 0; i < 10; ++i) {
        auto name = readSysfsAttr(current, "name");
        if (name)
            return *name;

        auto parent = getParentDevice(current);
        if (parent == current || parent.empty())
            break;
        current = std::move(parent);
    }
    return "Unknown";
}

} // namespace Kbi
