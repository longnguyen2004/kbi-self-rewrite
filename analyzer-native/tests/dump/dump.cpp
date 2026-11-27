#include <core/analyzer.h>
#include <core/result.h>
#include <parser/parser.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <print>
#include <string>
#include <thread>
#include <vector>

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

int main(int argc, char** argv)
{
    if (argc > 3 && std::string(argv[1]) == "--dump")
        return dumpAnalysis(argv[2], argv[3]);
    std::println(stderr, "Usage: analyzer-native-dump --dump <recording> <output>");
    return 1;
}
