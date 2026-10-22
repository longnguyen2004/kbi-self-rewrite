#pragma once

#include <string>
#include <string_view>

namespace Kbi {

// Returns the canonical name of a keycode (e.g. "A", "LeftControl",
// "LeftClick"). Unknown codes render as "Unknown".
[[nodiscard]] std::string_view keycodeName(int code);

// Windows virtual-key code -> keycode, used by the legacy `.kbi` parser.
[[nodiscard]] int vkToKeycode(int vk);

// Legacy key name -> Windows virtual-key code.
[[nodiscard]] int keyNameToVk(std::string_view name);

} // namespace Kbi
