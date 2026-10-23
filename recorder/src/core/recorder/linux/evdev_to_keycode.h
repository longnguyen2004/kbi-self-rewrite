#pragma once

#include <keycode.h>
#include <cstdint>

namespace Kbi {

[[nodiscard]] Keycode evdevToKeycode(std::uint16_t code);

} // namespace Kbi
