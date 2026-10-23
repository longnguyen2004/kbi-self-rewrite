#pragma once

#include <device.h>
#include <optional>
#include <string>

namespace Kbi {

[[nodiscard]] std::optional<std::string> findUsbDevice(std::string_view path);
[[nodiscard]] std::optional<UsbDeviceInfo> usbDeviceInfoFromPath(std::string_view path);
[[nodiscard]] std::string deviceNameFromPath(std::string_view syspath);

} // namespace Kbi
