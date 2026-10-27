#pragma once

#include <vector>

namespace Kbi {

struct ChartPoint {
    double x = 0.0;
    double y = 0.0;
};

// An inclusive range in chart x-units (I.24 — one strong type instead of two
// adjacent, swappable doubles).
struct AxisRange {
    double min = 0.0;
    double max = 0.0;
};

// Downsamples `data` over the visible `domain` (in chart x-units) into one
// point per pixel column of `width`, using bucket-max selection.
//
// `extent` describes the full x-range covered by `data` (e.g. [0, 1000] ms
// for the diff histograms, or [0, bin_rate / 2] Hz for the FFT). It is used
// to map between chart x-units and array indices, and to emit output x-values
// in the chart's own coordinate system.
//
// Zoom-stability: buckets are anchored to the **pixel grid** (one bucket per
// pixel column), not to a fixed point count derived from the visible range.
// For a given zoom level each pixel column always covers the same
// data-index span, so the bucket-max sample selected per column is
// deterministic and the line shape does not reshuffle as you pan within that
// zoom.
//
// The result is written into `out`, whose storage is reused across frames
// (Per.15 — the charts call this every frame). Any previous content of `out`
// is discarded.
void decimate(const std::vector<double>& data, AxisRange extent, AxisRange domain, int width,
    std::vector<ChartPoint>& out);

} // namespace Kbi
