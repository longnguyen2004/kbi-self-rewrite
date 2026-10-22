#include <device.h>
#include <optional>
#include <string>

std::optional<std::string> findUsbDevice(std::string_view path);
std::optional<UsbDeviceInfo> getUsbDeviceInfo(std::string_view path);
std::string deviceNameFromPath(std::string_view syspath);
