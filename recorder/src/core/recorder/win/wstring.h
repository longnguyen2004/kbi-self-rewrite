#pragma once

#include <string>
#include <string_view>

namespace Kbi {

[[nodiscard]] std::wstring stringToWstring(std::string_view str);
[[nodiscard]] std::string wstringToString(std::wstring_view wstr);

} // namespace Kbi
