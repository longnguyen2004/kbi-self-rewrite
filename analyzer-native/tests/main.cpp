#include <algorithm>
#include <chrono>
#include <core/analyzer.h>
#include <core/result.h>
#include <cstdio>
#include <fstream>
#include <memory>
#include <parser/parser.h>
#include <print>
#include <string>
#include <tests/tests.h>
#include <thread>
#include <vector>

namespace {

// Dumps the analysis of a recording so it can be compared against a reference
// implementation of the web analyzer.
int dumpAnalysis(const std::string& path, const std::string& outputPath)
{
    const auto result = Kbi::parseResultFile(path);
    if (!result) {
        std::println(stderr, "Could not parse {}", path);
        return 1;
    }

    std::vector<double> timestamps;
    for (const auto& [id, events] : result->inputs)
        for (const Kbi::InputEvent& event : events)
            timestamps.push_back(event.timestamp);
    std::sort(timestamps.begin(), timestamps.end());

    Kbi::Analyzer analyzer;
    analyzer.add(timestamps);

    // Wait for the worker thread to produce the frequency domain snapshot.
    std::shared_ptr<const Kbi::FreqSnapshot> snapshot;
    for (int i = 0; i < 1000 && !snapshot; ++i) {
        snapshot = analyzer.freqSnapshot();
        if (!snapshot)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!snapshot) {
        std::println(stderr, "The frequency snapshot was not ready");
        return 1;
    }

    std::ofstream out(outputPath);
    out.precision(17);
    if (!out) {
        std::println(stderr, "Could not write {}", outputPath);
        return 1;
    }

    const auto dump = [&out](const char* name, const std::vector<double>& values) {
        out << name << " " << values.size();
        for (const double value : values)
            out << " " << value;
        out << "\n";
    };

    dump("consecutive_diff", analyzer.consecutiveDiffCount());
    dump("all_diff", analyzer.allDiffCount());
    dump("wrapped_timestamp", analyzer.wrappedTimestampCount());
    dump("consecutive_diff_freq", snapshot->consecutiveDiff);
    dump("all_diff_freq", snapshot->allDiff);
    dump("wrapped_timestamp_freq", snapshot->wrappedTimestamp);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc > 3 && std::string(argv[1]) == "--dump")
        return dumpAnalysis(argv[2], argv[3]);

    Kbi::Tests::testFft();
    Kbi::Tests::testAnalyzer();
    Kbi::Tests::testRollingMean();
    Kbi::Tests::testDecimate();
    Kbi::Tests::testPostprocess();
    Kbi::Tests::testTimeline();
    Kbi::Tests::testBase64();
    Kbi::Tests::testDatetime();
    Kbi::Tests::testLogStore();
    Kbi::Tests::testResultDevices();
    Kbi::Tests::testUsbPollingRate();
    Kbi::Tests::testChartInteraction();
    Kbi::Tests::testPointSnapper();
    if (argc > 1)
        Kbi::Tests::testParser(argv[1]);
    if (argc > 2)
        Kbi::Tests::testLegacyParser(argv[2]);

    if (Kbi::Tests::failures() == 0) {
        std::println("\nAll tests passed.");
        return 0;
    }
    std::println("\n{} test(s) failed.", Kbi::Tests::failures());
    return 1;
}
