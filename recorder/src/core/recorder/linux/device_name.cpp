#include "device_name.h"
#include <systemd/sd-device.h>
#include <iostream>

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
