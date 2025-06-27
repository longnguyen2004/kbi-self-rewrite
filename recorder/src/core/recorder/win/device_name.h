#include <device.h>
#include <string>
#include <optional>

std::string hid_product_string_from_device_path(std::string_view id);
std::string hid_product_string_from_device_path(std::string_view id);

std::string hid_product_string_from_pnp(std::wstring_view wid);
std::string hid_product_string_from_pnp(std::string_view id);

std::optional<std::string> find_usb_device(std::wstring_view wid);
std::optional<std::string> find_usb_device(std::string_view id);

std::optional<UsbDeviceInfo> get_usb_device_info(std::wstring_view wid);
std::optional<UsbDeviceInfo> get_usb_device_info(std::string_view id);

std::string device_name_from_pnp(std::string_view pnp);
void device_info_from_pnp(std::string_view pnp);
