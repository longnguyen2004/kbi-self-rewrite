#pragma once

#include <keycode.h>
#include <cstdint>

Keycode evdev_to_keycode(std::uint16_t code);
