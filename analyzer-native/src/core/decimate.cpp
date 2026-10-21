#include "core/decimate.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace kbi {
namespace {

} // namespace

std::vector<ChartPoint> decimate(const std::vector<double>& data,
                                  double extent_min,
                                  double extent_max,
                                  double domain_min,
                                  double domain_max,
                                  int width) {
    std::vector<ChartPoint> points;
    const std::size_t n = data.size();
    if (n == 0 || width <= 0)
        return points;

    const double extent_span = extent_max - extent_min;
    // Degenerate extent: fall back to an identity mapping so we still emit
    // something rather than NaN-poisoning the path.
    const double denom = extent_span != 0.0 ? extent_span : 1.0;
    const double index_denom = n > 1 ? static_cast<double>(n - 1) : 1.0;

    const double x_min = std::min(domain_min, domain_max);
    const double x_max = std::max(domain_min, domain_max);
    const double domain_span = (x_max - x_min) != 0.0 ? (x_max - x_min) : 1.0;

    const auto to_index = [&](double x) -> long {
        const double raw = std::round(((x - extent_min) / denom) * index_denom);
        return static_cast<long>(std::clamp(raw, 0.0, static_cast<double>(n - 1)));
    };

    // Map the visible domain to pixel columns, then to data-index ranges.
    // One bucket per pixel column keeps the output count stable at `width`
    // and anchors bucket boundaries to the pixel grid (zoom-stable shape).
    const auto px_to_x = [&](double px) { return x_min + (px / width) * domain_span; };

    points.reserve(static_cast<std::size_t>(width));

    // Track the last emitted index to skip duplicate pixel columns (when many
    // pixels map to the same single data index, e.g. extreme zoom-in).
    long last_index = -1;
    for (int px = 0; px < width; ++px) {
        // Sample at the pixel's left edge; use the next pixel's left edge as
        // the bucket's right boundary so columns are contiguous and
        // non-overlapping.
        const double x_left = px_to_x(px);
        const double x_right = px_to_x(px + 1);
        const long i_start = to_index(x_left);
        const long i_end = std::max(i_start, to_index(x_right));

        // Skip pixel columns that map to an index we already emitted.
        if (i_start == last_index && i_end == last_index)
            continue;
        last_index = i_end;

        double bucket_max = -std::numeric_limits<double>::infinity();
        long bucket_max_index = i_start;
        for (long i = i_start; i <= i_end; ++i) {
            const double value = data[static_cast<std::size_t>(i)];
            if (value > bucket_max) {
                bucket_max = value;
                bucket_max_index = i;
            }
        }

        const double x = extent_min + (static_cast<double>(bucket_max_index) / index_denom) * denom;
        points.push_back(ChartPoint{x, bucket_max});
    }

    return points;
}

} // namespace kbi
