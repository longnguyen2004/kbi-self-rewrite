#pragma once

#include "core/result.h"

namespace kbi::ui {

// Draws the info window with the Summary / Devices / USB Devices tabs.
void draw_info_window(bool* open, const Result& result);

} // namespace kbi::ui
