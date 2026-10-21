#pragma once

#include <string>
#include <string_view>

namespace kbi {

// Returns the canonical name of a keycode (e.g. "A", "LeftControl",
// "LeftClick"). Unknown codes render as "Unknown".
[[nodiscard]] std::string_view keycode_name(int code);

// Windows virtual-key code -> keycode, used by the legacy `.kbi` parser.
[[nodiscard]] int vk_to_keycode(int vk);

// Legacy key name -> Windows virtual-key code.
[[nodiscard]] int key_name_to_vk(std::string_view name);

} // namespace kbi
