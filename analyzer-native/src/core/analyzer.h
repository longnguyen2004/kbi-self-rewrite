#pragma once

#include <core/result.h>
#include <core/rolling_mean.h>

#include <condition_variable>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

namespace Kbi {

// The frequency-domain snapshot of the three histograms.
struct FreqSnapshot {
    std::vector<double> consecutiveDiff;
    std::vector<double> allDiff;
    std::vector<double> wrappedTimestamp;
    double consecutiveDiffMax = 0.0;
    double allDiffMax = 0.0;
    double wrappedTimestampMax = 0.0;
};

// Port of the analyzer's `Analyzer` class.
//
// It keeps three histograms of the event timestamps:
//   * consecutive_diff - gap to the previous event
//   * all_diff         - gap to every previous event within the last second
//   * wrapped_timestamp- the fractional part of the timestamp within a second
//
// and derives their FFTs on a worker thread so the UI never blocks.
class Analyzer {
public:
    Analyzer();
    ~Analyzer();

    Analyzer(const Analyzer&) = delete;
    Analyzer& operator=(const Analyzer&) = delete;
    Analyzer(Analyzer&&) = delete;
    Analyzer& operator=(Analyzer&&) = delete;

    // Feeds a batch of timestamps (microseconds) into the histograms.
    void add(const std::vector<double>& timestamps);

    void reset();

    // The bin rate in Hz. Must be 125 * 2^n.
    [[nodiscard]] int binRate() const { return m_binRate; }
    void setBinRate(int binRate);

    [[nodiscard]] const std::vector<double>& consecutiveDiffCount() const
    {
        return m_consecutiveDiffCount;
    }
    [[nodiscard]] double consecutiveDiffCountMax() const { return m_consecutiveDiffCountMax; }
    // The shortest gap between two consecutive events in microseconds,
    // infinity when no gap was recorded yet.
    [[nodiscard]] double shortestConsecutiveGap() const { return m_shortestConsecutiveGap; }
    [[nodiscard]] const std::vector<double>& allDiffCount() const { return m_allDiffCount; }
    [[nodiscard]] double allDiffCountMax() const { return m_allDiffCountMax; }
    [[nodiscard]] const std::vector<double>& wrappedTimestampCount() const
    {
        return m_wrappedTimestampCount;
    }
    [[nodiscard]] double wrappedTimestampCountMax() const { return m_wrappedTimestampCountMax; }

    // Most recent frequency-domain snapshot; may be null before the first
    // calculation completes.
    [[nodiscard]] std::shared_ptr<const FreqSnapshot> freqSnapshot() const;

    // The highest frequency of the spectrum, in Hz.
    [[nodiscard]] double freqMaxHz() const { return m_binRate / 2.0; }

    [[nodiscard]] std::size_t eventCount() const { return m_eventCount; }
    [[nodiscard]] bool calculating() const;

    // The weighted mean of the FFT computation times, in milliseconds.
    [[nodiscard]] double averageFftTimeMs() const;

    // Stops the worker thread.
    void terminate();

private:
    void workerLoop(const std::stop_token& stop);
    void recompute();

    int m_binRate = 16000;
    std::vector<double> m_timestampRaw;
    std::vector<double> m_timestampBinned;

    std::vector<double> m_consecutiveDiffCount;
    std::vector<double> m_allDiffCount;
    std::vector<double> m_wrappedTimestampCount;
    double m_consecutiveDiffCountMax = 0.0;
    double m_allDiffCountMax = 0.0;
    double m_wrappedTimestampCountMax = 0.0;
    double m_shortestConsecutiveGap = std::numeric_limits<double>::infinity();

    std::size_t m_eventCount = 0;

    // Worker thread state.
    std::jthread m_thread;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;
    bool m_dirty = false;
    bool m_calculating = false;
    std::uint64_t m_generation = 0;
    std::uint64_t m_pendingGeneration = 0;
    std::vector<double> m_pendingConsecutive;
    std::vector<double> m_pendingAll;
    std::vector<double> m_pendingWrapped;
    std::shared_ptr<const FreqSnapshot> m_snapshot;
    // FFT runtimes accumulated on the worker thread, read under m_mutex from
    // the UI.
    RollingMean m_fftTime;
};

} // namespace Kbi
