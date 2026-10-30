#include "device_name.h"
#include <array>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <systemd/sd-device.h>
#include <unistd.h>
#include <vector>

namespace Kbi {

namespace {

struct SdDeviceCloser {
    void operator()(sd_device* device) const { sd_device_unref(device); }
};
// Owns the handle until scope exit.
using SdDevicePtr = std::unique_ptr<sd_device, SdDeviceCloser>;

// Open a device handle for a /dev node or a /sys path. The caller owns the
// returned handle and must sd_device_unref() it (or wrap it in SdDevicePtr).
sd_device* openDevice(std::string_view path)
{
    sd_device* device = nullptr;
    const std::string pathString(path);
    if (pathString.starts_with("/sys/"))
        sd_device_new_from_syspath(&device, pathString.c_str());
    else
        sd_device_new_from_devname(&device, pathString.c_str());
    return device;
}

std::optional<std::string> syspathOf(sd_device* device)
{
    const char* syspath = nullptr;
    if (sd_device_get_syspath(device, &syspath) < 0 || syspath == nullptr)
        return std::nullopt;
    return std::string(syspath);
}

} // namespace

std::optional<std::string> findUsbDevice(std::string_view path)
{
    SdDevicePtr device { openDevice(path) };
    if (!device)
        return {};

    // The usb_device ancestor of the input device chain.
    sd_device* usbDevice = nullptr;
    if (sd_device_get_parent_with_subsystem_devtype(
            device.get(), "usb", "usb_device", &usbDevice)
        < 0
        || usbDevice == nullptr)
        return {};
    // The reference is borrowed from the child, don't unref it.
    return syspathOf(usbDevice);
}

std::optional<UsbDeviceInfo> usbDeviceInfoFromPath(std::string_view path)
{
    SdDevicePtr device;
    {
        sd_device* deviceRaw = nullptr;
        if (sd_device_new_from_syspath(&deviceRaw, std::string(path).c_str()) < 0
            || deviceRaw == nullptr)
            return {};
        device.reset(deviceRaw);
    }

    UsbDeviceInfo devInfo;

    const char* value = nullptr;
    if (sd_device_get_sysattr_value(device.get(), "idVendor", &value) < 0 || value == nullptr)
        return {};
    const unsigned long vidValue = std::strtoul(value, nullptr, 16);
    if (vidValue > 0xFFFF)
        return {};
    devInfo.vid = static_cast<std::uint16_t>(vidValue);

    if (sd_device_get_sysattr_value(device.get(), "idProduct", &value) < 0 || value == nullptr)
        return {};
    const unsigned long pidValue = std::strtoul(value, nullptr, 16);
    if (pidValue > 0xFFFF)
        return {};
    devInfo.pid = static_cast<std::uint16_t>(pidValue);

    if (sd_device_get_sysattr_value(device.get(), "speed", &value) < 0 || value == nullptr)
        return {};
    if (std::string_view(value) == "1.5")
        devInfo.speed = UsbDeviceSpeed::LowSpeed;
    else if (std::string_view(value) == "12")
        devInfo.speed = UsbDeviceSpeed::FullSpeed;
    else if (std::string_view(value) == "480")
        devInfo.speed = UsbDeviceSpeed::HighSpeed;
    else
        devInfo.speed = UsbDeviceSpeed::SuperSpeed;

    // The descriptors attribute is binary; sd_device_get_sysattr_value()
    // stops at the first NUL byte, so read it via open().
    const auto syspath = syspathOf(device.get());
    if (!syspath)
        return devInfo;
    const auto descriptorPath = (std::filesystem::path(*syspath) / "descriptors").string();
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
    SdDevicePtr device { openDevice(path) };
    if (!device)
        return "Unknown";

    for (int i = 0; i < 10; ++i) {
        const char* name = nullptr;
        if (sd_device_get_sysattr_value(device.get(), "name", &name) >= 0 && name != nullptr
            && *name != '\0')
            return name;

        // The reference is borrowed from the child, don't unref it.
        sd_device* parent = nullptr;
        if (sd_device_get_parent(device.get(), &parent) < 0 || parent == nullptr)
            break;
        SdDevicePtr held { sd_device_ref(parent) };
        device = std::move(held);
    }
    return "Unknown";
}

} // namespace Kbi
