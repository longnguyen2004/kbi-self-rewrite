#include "core/fft.h"

#include <pffft/pffft.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <memory>
#include <new>
#include <vector>

namespace Kbi::Fft {
namespace {

// The setup of PFFFT owns the twiddle factors, it has to be destroyed.
struct PffftSetupDeleter {
    void operator()(PFFFT_Setup* setup) const noexcept { pffft_destroy_setup(setup); }
};

using PffftSetup = std::unique_ptr<PFFFT_Setup, PffftSetupDeleter>;

// The buffers of PFFFT have to be aligned, 16 bytes on x86 and powerpc.
template<typename T> struct PffftAllocator {
    using value_type = T;

    PffftAllocator() = default;

    template<typename U> explicit PffftAllocator(const PffftAllocator<U>& /*unused*/) { }

    [[nodiscard]] T* allocate(std::size_t count)
    {
        void* memory = pffft_aligned_malloc(count * sizeof(T));
        if (memory == nullptr)
            throw std::bad_alloc();
        return static_cast<T*>(memory);
    }

    void deallocate(T* memory, std::size_t /*unused*/) noexcept { pffft_aligned_free(memory); }

    bool operator==(const PffftAllocator& /*unused*/) const { return true; }
};

// PFFFT supports transform sizes of the form 2^a * 3^b * 5^c with a >= 5.
bool isSupportedSize(std::size_t size)
{
    int twos = 0;
    while (size % 2 == 0) {
        size /= 2;
        ++twos;
    }
    while (size % 3 == 0)
        size /= 3;
    while (size % 5 == 0)
        size /= 5;
    return size == 1 && twos >= 5;
}

// The size of the transform. The histograms have one entry per hertz of the
// spectrum, their length is the bin rate (125 * 2^n), which PFFFT supports
// directly. Only the small bin rates need to be rounded up.
std::size_t transformSizeFor(std::size_t length)
{
    if (isSupportedSize(length))
        return length;
    // The next power of two is always supported.
    return std::bit_ceil(std::max<std::size_t>(length, 32));
}

} // namespace

RealSpectrum realDft(const std::vector<double>& in)
{
    RealSpectrum result;

    const std::size_t n = in.size();
    if (n == 0)
        return result;

    const std::size_t m = transformSizeFor(n);

    PffftSetup setup(pffft_new_setup(static_cast<int>(m), PFFFT_REAL));
    if (!setup)
        return result;

    // The buffers are zero initialized, so the part of the signal that is
    // padded is already zero.
    std::vector<float, PffftAllocator<float>> input(m);
    std::vector<float, PffftAllocator<float>> output(4 * m);
    std::vector<float, PffftAllocator<float>> work(2 * m);

    for (std::size_t i = 0; i < n; ++i)
        input[i] = static_cast<float>(in[i]);

    pffft_transform_ordered(setup.get(), input.data(), output.data(), work.data(), PFFFT_FORWARD);

    // For a real transform PFFFT packs the (real) Nyquist component into the
    // imaginary part of the first entry, it is moved to its own entry here.
    const std::size_t bins = m / 2 + 1;
    result.transformSize = m;
    result.values.resize(bins * 2);
    result.values[0] = output[0];
    result.values[1] = 0.0;
    for (std::size_t k = 1; k + 1 < bins; ++k) {
        result.values[k * 2] = output[k * 2];
        result.values[k * 2 + 1] = output[k * 2 + 1];
    }
    result.values[(bins - 1) * 2] = output[1];
    result.values[(bins - 1) * 2 + 1] = 0.0;

    return result;
}

} // namespace Kbi::Fft
