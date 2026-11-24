#pragma once

#include <string>

namespace Kbi {

// Formats a number the way JavaScript would, so the values match the ones
// the web analyzer displays.
std::string formatNumber(double value);

// Formats a duration in microseconds, switching to milliseconds at 1ms and up.
std::string formatDuration(double microseconds);

} // namespace Kbi
