#pragma once

#include "core/result.h"

namespace Kbi::Ui {

// Draws the info window with the Summary / Devices / USB Devices tabs.
void drawInfoWindow(bool* open, const Result& result);

} // namespace Kbi::ui
