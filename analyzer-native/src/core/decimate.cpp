#include <core/decimate.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Kbi {
namespace {

} // namespace

void decimate(const std::vector<double>& data, AxisRange extent, AxisRange domain, int width,
    std::vector<ChartPoint>& out)
{
    out.clear();
    const std::size_t n = data.size();
    if (n == 0 || width <= 0)
        return;

    const double extentSpan = extent.max - extent.min;
    // Degenerate extent: fall back to an identity mapping so we still emit
    // something rather than NaN-poisoning the path.
    const double denom = extentSpan != 0.0 ? extentSpan : 1.0;
    const double indexDenom = n > 1 ? static_cast<double>(n - 1) : 1.0;

    const double xMin = std::min(domain.min, domain.max);
    const double xMax = std::max(domain.min, domain.max);
    const double domainSpan = (xMax - xMin) != 0.0 ? (xMax - xMin) : 1.0;

    const auto toIndex = [&](double x) -> long {
        const double raw = std::round(((x - extent.min) / denom) * indexDenom);
        return static_cast<long>(std::clamp(raw, 0.0, static_cast<double>(n - 1)));
    };

    // Map the visible domain to pixel columns, then to data-index ranges.
    // One bucket per pixel column keeps the output count stable at `width`
    // and anchors bucket boundaries to the pixel grid (zoom-stable shape).
    const auto pxToX = [&](double px) { return xMin + (px / width) * domainSpan; };

    out.reserve(static_cast<std::size_t>(width));

    // Track the last emitted index to skip duplicate pixel columns (when many
    // pixels map to the same single data index, e.g. extreme zoom-in).
    long lastIndex = -1;
    for (int px = 0; px < width; ++px) {
        // Sample at the pixel's left edge; use the next pixel's left edge as
        // the bucket's right boundary so columns are contiguous and
        // non-overlapping.
        const double xLeft = pxToX(px);
        const double xRight = pxToX(px + 1);
        const long iStart = toIndex(xLeft);
        const long iEnd = std::max(iStart, toIndex(xRight));

        // Skip pixel columns that map to an index we already emitted.
        if (iStart == lastIndex && iEnd == lastIndex)
            continue;
        lastIndex = iEnd;

        double bucketMax = -std::numeric_limits<double>::infinity();
        long bucketMaxIndex = iStart;
        for (long i = iStart; i <= iEnd; ++i) {
            const double value = data[static_cast<std::size_t>(i)];
            if (value > bucketMax) {
                bucketMax = value;
                bucketMaxIndex = i;
            }
        }

        const double x = extent.min + (static_cast<double>(bucketMaxIndex) / indexDenom) * denom;
        out.push_back(ChartPoint { .x = x, .y = bucketMax });
    }
}

} // namespace Kbi
