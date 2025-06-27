#include "device_name.h"
#include <systemd/sd-device.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <vector>
#include <tuple>
#include <fcntl.h>

std::tuple<const char*, const char*> subsys_driver_chain[] = {
    { "input", "" },
    { "input", "" },
    { "hid", "hid-generic" },
    { "usb", "usbhid" },
};

std::optional<std::string> find_usb_device(std::string_view path)
{
    sd_device *current = nullptr;
    if (sd_device_new_from_path(&current, path.data()) != 0)
    {
        return {};
    }

    // Check the chain and see if it matches the expected chain for a USB HID device
    for (auto& [subsys, driver]: subsys_driver_chain)
    {
        const char *device_subsys, *device_driver = "";
        if (sd_device_get_subsystem(current, &device_subsys) != 0)
            return {};
        if (auto ret = sd_device_get_driver(current, &device_driver); ret != 0 && ret != -ENOENT)
            return {};
        if (std::strcmp(subsys, device_subsys) != 0)
            return {};
        if (std::strcmp(driver, device_driver) != 0)
            return {};
        sd_device* temp = current;
        sd_device_get_parent(temp, &current);
    }

    // current will be the root USB device
    const char* syspath;
    sd_device_get_syspath(current, &syspath);
    return syspath;
}

std::optional<UsbDeviceInfo> get_usb_device_info(std::string_view path)
{
    sd_device *device = nullptr;
    if (sd_device_new_from_path(&device, path.data()) != 0)
    {
        return {};
    }
    UsbDeviceInfo devInfo;
    const char* str;
    
    if (sd_device_get_sysattr_value(device, "idVendor", &str) != 0)
    {
        return {};
    }
    devInfo.VID = std::strtoul(str, nullptr, 16);
    if (sd_device_get_sysattr_value(device, "idProduct", &str) != 0)
    {
        return {};
    }
    devInfo.PID = std::strtoul(str, nullptr, 16);
    if (sd_device_get_sysattr_value(device, "speed", &str) != 0)
    {
        return {};
    }
    if (strcmp(str, "1.5") == 0)
        devInfo.Speed = UsbDeviceSpeed::LOW_SPEED;
    else if (strcmp(str, "12") == 0)
        devInfo.Speed = UsbDeviceSpeed::FULL_SPEED;
    else if (strcmp(str, "480") == 0)
        devInfo.Speed = UsbDeviceSpeed::HIGH_SPEED;
    else
        devInfo.Speed = UsbDeviceSpeed::SUPERSPEED;

    auto descriptor_path = std::format("{}/descriptors", path);
    auto fd = open(descriptor_path.c_str(), O_RDONLY);
    if (fd)
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
    sd_device *device = nullptr;
    __attribute__((cleanup(sd_device_unrefp))) sd_device *parent = nullptr;

    if (sd_device_new_from_path(&device, path.data()) != 0)
    {
        return "Unknown";
    }
    if (sd_device_get_parent(device, &parent) != 0)
    {
        return "Unknown";
    }
    const char* name;
    if (sd_device_get_sysattr_value(parent, "name", &name) != 0)
    {
        return "Unknown";
    }
    std::string name_str(name);
    return name_str;
}
