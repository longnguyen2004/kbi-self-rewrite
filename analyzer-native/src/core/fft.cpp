#include <core/fft.h>

#include <pffft/pffft.h>

#include <algorithm>
#include <bit>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace Kbi::Fft {
namespace {

// The setup of PFFFT owns the twiddle factors, it has to be destroyed.
struct PffftSetupDeleter {
    void operator()(PFFFT_Setup* setup) const noexcept { pffft_destroy_setup(setup); }
};

using PffftSetup = std::unique_ptr<PFFFT_Setup, PffftSetupDeleter>;

// The buffers of PFFFT have to be aligned, 16 bytes on x86 and powerpc.
// The element types are constrained to object types (T.10).
template<typename T>
    requires std::is_object_v<T>
struct PffftAllocator {
    using value_type = T;

    PffftAllocator() = default;

    template<typename U>
        requires std::is_object_v<U>
    explicit PffftAllocator(const PffftAllocator<U>& /*unused*/)
    {
    }

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

// The setup owns the twiddle factors and the buffers are aligned
// allocations; both are expensive to build, so they are cached per transform
// size and per thread (Per.14 — the analyzer runs three transforms per
// snapshot on its worker thread).
struct PffftWorkspace {
    PffftSetup setup;
    std::vector<float, PffftAllocator<float>> input;
    std::vector<float, PffftAllocator<float>> output;
    std::vector<float, PffftAllocator<float>> work;
};

PffftWorkspace& workspaceFor(std::size_t m)
{
    static thread_local std::unordered_map<std::size_t, PffftWorkspace> cache;
    auto [it, inserted] = cache.try_emplace(m);
    if (inserted) {
        it->second.setup.reset(pffft_new_setup(static_cast<int>(m), PFFFT_REAL));
        it->second.input.resize(m);
        it->second.output.resize(4 * m);
        it->second.work.resize(2 * m);
    }
    return it->second;
}

} // namespace

RealSpectrum realDft(const std::vector<double>& in)
{
    RealSpectrum result;

    const std::size_t n = in.size();
    if (n == 0)
        return result;

    const std::size_t m = transformSizeFor(n);

    PffftWorkspace& workspace = workspaceFor(m);
    if (!workspace.setup)
        return result;

    // The padding after the signal has to be zero on every call, the buffer
    // is reused across calls.
    std::fill(workspace.input.begin(), workspace.input.end(), 0.0f);
    for (std::size_t i = 0; i < n; ++i)
        workspace.input[i] = static_cast<float>(in[i]);

    pffft_transform_ordered(workspace.setup.get(), workspace.input.data(), workspace.output.data(),
        workspace.work.data(), PFFFT_FORWARD);

    // For a real transform PFFFT packs the (real) Nyquist component into the
    // imaginary part of the first entry, it is moved to its own entry here.
    const std::size_t bins = m / 2 + 1;
    result.transformSize = m;
    result.values.resize(bins * 2);
    result.values[0] = workspace.output[0];
    result.values[1] = 0.0;
    for (std::size_t k = 1; k + 1 < bins; ++k) {
        result.values[k * 2] = workspace.output[k * 2];
        result.values[k * 2 + 1] = workspace.output[k * 2 + 1];
    }
    result.values[(bins - 1) * 2] = workspace.output[1];
    result.values[(bins - 1) * 2 + 1] = 0.0;

    return result;
}

} // namespace Kbi::Fft
