#include <core/format.h>

#include <cmath>
#include <format>

namespace Kbi {

std::string formatNumber(double value)
{
    if (std::isinf(value))
        return value > 0 ? "Infinity" : "-Infinity";
    if (std::isnan(value))
        return "NaN";
    // The default format uses the shortest representation that round trips.
    return std::format("{}", value);
}

std::string formatDuration(double microseconds)
{
    if (microseconds >= 1000.0)
        return std::format("{}ms", formatNumber(microseconds / 1000.0));
    return std::format("{}\u03bcs", formatNumber(microseconds));
}

} // namespace Kbi
