#pragma once

#include <cstddef>
#include <vector>

namespace Kbi::Fft {

// The spectrum of a real signal.
struct RealSpectrum {
    // The size of the transform. The signal is zero padded to this size.
    std::size_t transformSize = 0;
    // The spectrum, (transform_size / 2 + 1) complex values interleaved as
    // [re0, im0, re1, im1, ...].
    std::vector<double> values;
};

// Unnormalized real-input DFT.
//
// PFFFT only supports transform sizes of the form 2^a * 3^b * 5^c, so the
// signal is zero padded to the next power of two. The frequency of the bin k
// is therefore k * sample_rate / transform_size.
//
// The result is not normalized, matching the convention used by the web
// analyzer (the magnitude is scaled by 1/N or 2/N by the caller).
RealSpectrum realDft(const std::vector<double>& in);

} // namespace Kbi::Fft
