#include <boost/ut.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <complex>
#include <core/fft.h>
#include <format>
#include <numbers>
#include <print>
#include <random>
#include <utility>
#include <vector>

namespace {
// Reference DFT of the zero padded signal, computed directly. O(n^2) but only
// used for small inputs.
std::vector<std::complex<double>> naiveDft(const std::vector<double>& input, std::size_t size)
{
    std::vector<std::complex<double>> result(size);
    for (std::ptrdiff_t k = 0; std::cmp_less(k, size); ++k) {
        std::complex<double> sum(0.0, 0.0);
        for (std::size_t j = 0; j < input.size(); ++j) {
            const double angle = -2.0 * std::numbers::pi
                * static_cast<double>(static_cast<std::size_t>(k) * j) / static_cast<double>(size);
            sum += input[j] * std::complex<double>(std::cos(angle), std::sin(angle));
        }
        result[static_cast<std::size_t>(k)] = sum;
    }
    return result;
}
} // namespace

int main()
{
    using namespace boost::ut;

    "fft"_test = [] {
        // A fixed seed keeps the test deterministic.
        std::mt19937 rng(1234); // NOLINT(bugprone-random-generator-seed)

        for (const std::size_t n : { 1u, 2u, 3u, 5u, 16u, 31u }) {
            std::vector<double> input(n);
            for (double& value : input)
                value = static_cast<double>(rng() % 1000) / 1000.0;

            const auto spectrum = Kbi::Fft::realDft(input);
            const std::size_t m = spectrum.transformSize;
            expect(m == std::bit_ceil(std::max<std::size_t>(n, 32)))
                << std::format("the transform size is valid for n={}", n);
            expect(spectrum.values.size() == (m / 2 + 1) * 2)
                << std::format("the spectrum has one bin per frequency for n={}", n);

            // The reference is the dft of the zero padded signal.
            const auto reference = naiveDft(input, m);

            double worst = 0.0;
            for (std::ptrdiff_t k = 0; std::cmp_less(k, m / 2 + 1); ++k) {
                const std::complex<double> got(spectrum.values[static_cast<std::size_t>(k * 2)],
                    spectrum.values[static_cast<std::size_t>(k * 2 + 1)]);
                worst = std::max(worst, std::abs(got - reference[static_cast<std::size_t>(k)]));
            }
            // PFFFT works in single precision.
            expect(worst < 1e-4) << std::format(
                "the spectrum matches the reference dft for n={} (err={})", n, worst);
        }

        // A pure tone must show up as a single bin.
        constexpr std::size_t m = 16384;
        std::vector<double> tone(m);
        for (std::size_t i = 0; i < m; ++i) {
            tone[i] = std::cos(
                2.0 * std::numbers::pi * 100.0 * static_cast<double>(i) / static_cast<double>(m));
        }
        const auto spectrum = Kbi::Fft::realDft(tone);
        std::size_t peak = 0;
        double peakValue = 0.0;
        for (std::ptrdiff_t k = 0; std::cmp_less(k, spectrum.values.size() / 2); ++k) {
            const double magnitude = std::hypot(spectrum.values[static_cast<std::size_t>(k * 2)],
                spectrum.values[static_cast<std::size_t>(k * 2 + 1)]);
            if (magnitude > peakValue) {
                peakValue = magnitude;
                peak = static_cast<std::size_t>(k);
            }
        }
        expect(peak == 100) << std::format("a 100Hz tone peaks at bin 100 (got {})", peak);

        // The transform is not padded when PFFFT supports the size, the bin rate
        // is 125 * 2^n.
        const std::vector<double> binRate(16000, 1.0);
        expect(Kbi::Fft::realDft(binRate).transformSize == 16000)
            << "the transform of a bin rate sized signal is not padded";

        // A size that PFFFT does not support is rounded up to a power of two.
        const std::vector<double> small(1000, 1.0);
        expect(Kbi::Fft::realDft(small).transformSize == 1024)
            << "an unsupported size is rounded up";
        expect(std::abs(peakValue - static_cast<double>(m) / 2.0) < 1.0)
            << "the magnitude of the tone is m/2";
    };
}
