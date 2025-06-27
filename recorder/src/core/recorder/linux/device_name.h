#include <device.h>
#include <string>
#include <optional>

std::optional<std::string> find_usb_device(std::string_view path);
std::optional<UsbDeviceInfo> get_usb_device_info(std::string_view path);
std::string device_name_from_path(std::string_view syspath);
