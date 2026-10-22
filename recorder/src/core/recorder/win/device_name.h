#include <device.h>
#include <optional>
#include <string>

std::string hidProductStringFromDevicePath(std::wstring_view wid);
std::string hidProductStringFromDevicePath(std::string_view id);

std::string hidProductStringFromPnp(std::wstring_view wid);
std::string hidProductStringFromPnp(std::string_view id);

std::optional<std::string> findUsbDevice(std::wstring_view wid);
std::optional<std::string> findUsbDevice(std::string_view id);

std::optional<UsbDeviceInfo> usbDeviceInfoFromPath(std::wstring_view wid);
std::optional<UsbDeviceInfo> usbDeviceInfoFromPath(std::string_view id);

std::string deviceNameFromPnp(std::string_view id);
void deviceInfoFromPnp(std::string_view id);
