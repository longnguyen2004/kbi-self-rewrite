#include <string>

std::string hid_product_string_from_device_path(std::string_view id);
std::string device_name_from_pnp(std::string_view pnp);
void device_info_from_pnp(std::string_view pnp);
