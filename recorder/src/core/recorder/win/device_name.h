#pragma once

#include <device.h>
#include <optional>
#include <string>

namespace Kbi {

[[nodiscard]] std::string hidProductStringFromDevicePath(std::wstring_view wid);
[[nodiscard]] std::string hidProductStringFromDevicePath(std::string_view id);

[[nodiscard]] std::string hidProductStringFromPnp(std::wstring_view wid);
[[nodiscard]] std::string hidProductStringFromPnp(std::string_view id);

[[nodiscard]] std::optional<std::string> findUsbDevice(std::wstring_view wid);
[[nodiscard]] std::optional<std::string> findUsbDevice(std::string_view id);

[[nodiscard]] std::optional<UsbDeviceInfo> usbDeviceInfoFromPath(std::wstring_view wid);
[[nodiscard]] std::optional<UsbDeviceInfo> usbDeviceInfoFromPath(std::string_view id);

[[nodiscard]] std::string deviceNameFromPnp(std::string_view id);

} // namespace Kbi
