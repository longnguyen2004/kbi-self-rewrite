#include <boost/ut.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <core/analyzer.h>
#include <format>
#include <memory>
#include <print>
#include <thread>
#include <vector>

int main()
{
    using namespace boost::ut;

    "analyzer"_test = [] {
        Kbi::Analyzer analyzer;
        const int binRate = analyzer.binRate();

        // A perfectly periodic event train: every 10ms. The first event is not
        // at zero, otherwise the web analyzer skips its diff as well.
        std::vector<double> timestamps;
        timestamps.reserve(200);
        for (int i = 0; i < 200; ++i)
            timestamps.push_back(1000.0 + static_cast<double>(i) * 10000.0);
        analyzer.add(timestamps);

        expect(analyzer.eventCount() == 200) << "event count";

        const double interval = 1000000.0 / binRate;
        const auto index = static_cast<std::size_t>(std::llround(10000.0 / interval));
        expect(analyzer.consecutiveDiffCount()[index] == 199.0)
            << "consecutive diff count histogram counts the 10ms gaps";
        expect(analyzer.consecutiveDiffCountMax() == 199.0) << "consecutive diff count max";
        expect(analyzer.shortestConsecutiveGap() == 10000.0)
            << "shortest consecutive gap of the 10ms gaps";

        // A 10ms event train is a 100Hz rhythm, the spectrum has to peak at
        // 100Hz. The transform runs at the bin rate, so the frequency of a bin is
        // its index in hertz.
        {
            std::shared_ptr<const Kbi::FreqSnapshot> snapshot;
            for (int i = 0; i < 1000 && !snapshot; ++i) {
                snapshot = analyzer.freqSnapshot();
                if (!snapshot)
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            expect(snapshot != nullptr) << "the frequency snapshot is ready";
            if (snapshot) {
                // The differences to all the previous events form a comb with a
                // period of 10ms, its spectrum peaks at 100Hz.
                std::size_t peak = 0;
                double peakValue = 0.0;
                // Skip the direct current component.
                for (std::size_t k = 1; k < snapshot->allDiff.size(); ++k) {
                    if (snapshot->allDiff[k] > peakValue) {
                        peakValue = snapshot->allDiff[k];
                        peak = k;
                    }
                }
                expect(peak == 100)
                    << std::format("the 100Hz rhythm peaks at 100Hz (got {})", peak);
            }
        }

        // The spectrum covers every frequency from 0 to bin_rate / 2 Hz, which is
        // bin_rate / 2 + 1 entries. The transform runs at the bin rate, so the
        // frequency of a bin is its index in hertz.
        for (const int rate : { 1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000 }) {
            Kbi::Analyzer rateAnalyzer;
            rateAnalyzer.setBinRate(rate);
            std::vector<double> stamps;
            stamps.reserve(100);
            for (int i = 0; i < 100; ++i)
                stamps.push_back(static_cast<double>(i) * 1000000.0 / rate);
            rateAnalyzer.add(stamps);

            std::shared_ptr<const Kbi::FreqSnapshot> snapshot;
            for (int i = 0; i < 1000 && !snapshot; ++i) {
                snapshot = rateAnalyzer.freqSnapshot();
                if (!snapshot)
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            expect(snapshot != nullptr)
                << std::format("the frequency snapshot of a {}Hz bin rate is ready", rate);
            if (snapshot) {
                // PFFFT needs a transform size that is a multiple of 32, the two
                // small bin rates are rounded up to the next power of two and
                // their spectrum has a few entries above the axis.
                const std::size_t transformSize = rate % 32 == 0
                    ? static_cast<std::size_t>(rate)
                    : std::bit_ceil(static_cast<unsigned>(rate));
                expect(snapshot->consecutiveDiff.size() == transformSize / 2 + 1) << std::format(
                    "a {}Hz bin rate has {} frequency entries", rate, transformSize / 2 + 1);
            }
        }

        // Non monotonic timestamps are ignored.
        analyzer.add({ 1000 });
        expect(analyzer.eventCount() == 200) << "non monotonic timestamps are ignored";

        // Changing the bin rate re-ingests the raw timestamps.
        analyzer.setBinRate(8000);
        expect(analyzer.binRate() == 8000) << "bin rate changed";
        expect(analyzer.eventCount() == 200) << "event count after the bin rate change";
        constexpr double newInterval = 1000000.0 / 8000.0;
        const auto newIndex = static_cast<std::size_t>(std::llround(10000.0 / newInterval));
        expect(analyzer.consecutiveDiffCount()[newIndex] == 199.0)
            << "consecutive diff count histogram after the bin rate change";

        // Invalid bin rates are rejected.
        analyzer.setBinRate(999);
        expect(analyzer.binRate() == 8000) << "bin rate that is not a multiple of 125 is rejected";
        analyzer.setBinRate(1500);
        expect(analyzer.binRate() == 8000) << "bin rate that is not 125 * 2^n is rejected";
        analyzer.setBinRate(-1);
        expect(analyzer.binRate() == 8000) << "negative bin rate is rejected";
        analyzer.setBinRate(1000);
        expect(analyzer.binRate() == 1000) << "valid bin rate accepted";

        // The shortest gap between two consecutive events is tracked; the events
        // are beyond the periodic train, whose last event is at 1992000µs.
        analyzer.add({ 1992000 + 5000, 1992000 + 6000 });
        expect(analyzer.shortestConsecutiveGap() == 1000.0)
            << "shortest consecutive gap tracks the shortest gap between events";

        // Without events there is no gap to measure.
        analyzer.reset();
        expect(std::isinf(analyzer.shortestConsecutiveGap()))
            << "shortest consecutive gap is infinite without events";
    };
}
