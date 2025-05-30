#pragma once

#include <keycode.h>
#include <cstdint>

Keycode evdev_to_hid(std::uint16_t code);
