#pragma once

#include <vector>

namespace Kbi {

struct ChartPoint {
    double x = 0.0;
    double y = 0.0;
};

// Downsamples `data` over the visible `domain` (in chart x-units) into one
// point per pixel column of `width`, using bucket-max selection.
//
// `extent_min`/`extent_max` describe the full x-range covered by `data`
// (e.g. [0, 1000] ms for the diff histograms, or [0, bin_rate / 2] Hz for
// the FFT). They are used to map between chart x-units and array indices,
// and to emit output x-values in the chart's own coordinate system.
//
// Zoom-stability: buckets are anchored to the **pixel grid** (one bucket per
// pixel column), not to a fixed point count derived from the visible range.
// For a given zoom level each pixel column always covers the same
// data-index span, so the bucket-max sample selected per column is
// deterministic and the line shape does not reshuffle as you pan within that
// zoom.
std::vector<ChartPoint> decimate(const std::vector<double>& data,
    double extentMin,
    double extentMax,
    double domainMin,
    double domainMax,
    int width);

} // namespace Kbi
