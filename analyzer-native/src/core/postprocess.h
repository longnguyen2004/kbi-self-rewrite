#pragma once

#include <vector>

namespace kbi {

struct PostprocessOptions {
    bool low_cut = true;
};

struct PostprocessResult {
    std::vector<double> data;
    double max = 0.0;
};

// Applies the optional "low cut" (high-pass) filter and computes the
// maximum of the series.
//
// The low cut divides the first bins by a logistic function centred at
// bin 25, which suppresses the DC component and everything below roughly
// 25 Hz. It is only meaningful in the frequency domain.
PostprocessResult postprocess_with_max(const std::vector<double>& arr,
                                       const PostprocessOptions& options);

} // namespace kbi
