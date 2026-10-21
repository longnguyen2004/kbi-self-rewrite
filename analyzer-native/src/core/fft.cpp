#include "core/fft.h"

#include <pffft/pffft.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <vector>

namespace kbi::fft {

RealSpectrum real_dft(const std::vector<double>& in) {
    RealSpectrum result;

    const std::size_t n = in.size();
    if (n == 0)
        return result;

    // PFFFT supports transform sizes of the form 2^a * 3^b * 5^c with a >= 5,
    // so the signal is zero padded to the next power of two.
    const std::size_t m = std::bit_ceil(std::max<std::size_t>(n, 32));

    PFFFT_Setup* setup = pffft_new_setup(static_cast<int>(m), PFFFT_REAL);
    if (!setup)
        return result;

    float* input = static_cast<float*>(pffft_aligned_malloc(m * sizeof(float)));
    float* output = static_cast<float*>(pffft_aligned_malloc(4 * m * sizeof(float)));
    float* work = static_cast<float*>(pffft_aligned_malloc(2 * m * sizeof(float)));

    std::fill_n(input, m, 0.0f);
    for (std::size_t i = 0; i < n; ++i)
        input[i] = static_cast<float>(in[i]);

    pffft_transform_ordered(setup, input, output, work, PFFFT_FORWARD);

    // For a real transform PFFFT packs the (real) Nyquist component into the
    // imaginary part of the first entry, it is moved to its own entry here.
    const std::size_t bins = m / 2 + 1;
    result.transform_size = m;
    result.values.resize(bins * 2);
    result.values[0] = output[0];
    result.values[1] = 0.0;
    for (std::size_t k = 1; k + 1 < bins; ++k) {
        result.values[k * 2] = output[k * 2];
        result.values[k * 2 + 1] = output[k * 2 + 1];
    }
    result.values[(bins - 1) * 2] = output[1];
    result.values[(bins - 1) * 2 + 1] = 0.0;

    pffft_aligned_free(work);
    pffft_aligned_free(output);
    pffft_aligned_free(input);
    pffft_destroy_setup(setup);

    return result;
}

} // namespace kbi::fft
