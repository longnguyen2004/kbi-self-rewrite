#include <tests/tests.h>

#include <algorithm>
#include <core/decimate.h>
#include <print>
#include <ui/charts.h>
#include <utility>
#include <vector>

namespace Kbi::Tests {
void testDecimate()
{
    std::println("decimate");

    std::vector<double> data(1001);
    for (std::ptrdiff_t i = 0; std::cmp_less(i, data.size()); ++i)
        data[static_cast<std::size_t>(i)] = static_cast<double>(i % 7);

    std::vector<Kbi::ChartPoint> points;
    Kbi::decimate(data, { .min = 0.0, .max = 1000.0 }, { .min = 0.0, .max = 1000.0 }, 100,
        Kbi::SampleLayout::Point, points);
    check(points.size() <= 100, "at most one point per pixel column");
    check(!points.empty(), "points are produced");

    // The maximum of the whole range must be preserved by the bucket max.
    double expectedMax = 0.0;
    for (const double value : data)
        expectedMax = std::max(expectedMax, value);
    double actualMax = 0.0;
    for (const auto& point : points)
        actualMax = std::max(actualMax, point.y);
    check(actualMax == expectedMax, "the bucket max preserves the global maximum");

    // The point layout places the last entry on the extent max, like the last
    // frequency bin of a spectrum.
    std::vector<double> spectrum(513, 0.0);
    spectrum[512] = 3.0;
    Kbi::decimate(spectrum, { .min = 0.0, .max = 512.0 }, { .min = 0.0, .max = 512.0 }, 100,
        Kbi::SampleLayout::Point, points);
    bool foundLastBin = false;
    for (const auto& point : points) {
        if (point.y == 3.0) {
            foundLastBin = true;
            check(point.x == 512.0, "the last point sample sits on the extent max");
        }
    }
    check(foundLastBin, "the last point sample is drawn");

    // Zooming in must not produce points outside of the visible domain.
    Kbi::decimate(data, { .min = 0.0, .max = 1000.0 }, { .min = 500.0, .max = 510.0 }, 100,
        Kbi::SampleLayout::Point, points);
    for (const auto& point : points)
        check(point.x >= 490.0 && point.x <= 520.0, "zoomed points stay in range");

    // The diff histograms are buckets, not point samples: a 1000Hz bin rate
    // gives one 1ms bucket per entry, so entry i sits at i ms. Mapping them
    // like point samples would place entry i at i * 1000 / (n - 1) ms, which
    // drifts away from the bin rate the larger i gets.
    std::vector<double> histogram(1000, 0.0);
    histogram[1] = 5.0;
    histogram[999] = 7.0;
    Kbi::decimate(histogram, { .min = 0.0, .max = 1000.0 }, { .min = 0.0, .max = 1000.0 }, 100,
        Kbi::SampleLayout::Bucket, points);
    bool foundFirstBucket = false;
    bool foundLastBucket = false;
    for (const auto& point : points) {
        if (point.y == 5.0) {
            foundFirstBucket = true;
            check(point.x == 1.0, "the first 1ms bucket sits at 1ms");
        }
        if (point.y == 7.0) {
            foundLastBucket = true;
            check(point.x == 999.0, "the last bucket sits at its start, not at the extent max");
        }
    }
    check(foundFirstBucket, "the first bucket is drawn");
    check(foundLastBucket, "the last bucket is drawn");
}

} // namespace Kbi::Tests
