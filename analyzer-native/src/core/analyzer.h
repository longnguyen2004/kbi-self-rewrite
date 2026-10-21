#pragma once

#include "core/result.h"

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace kbi {

// The frequency-domain snapshot of the three histograms.
struct FreqSnapshot {
    std::vector<double> consecutive_diff;
    std::vector<double> all_diff;
    std::vector<double> wrapped_timestamp;
    double consecutive_diff_max = 0.0;
    double all_diff_max = 0.0;
    double wrapped_timestamp_max = 0.0;
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

    // Feeds a batch of timestamps (microseconds) into the histograms.
    void add(const std::vector<double>& timestamps);

    void reset();

    // The bin rate in Hz. Must be 125 * 2^n.
    [[nodiscard]] int bin_rate() const { return m_bin_rate; }
    void set_bin_rate(int bin_rate);

    [[nodiscard]] const std::vector<double>& consecutive_diff() const { return m_consecutive_diff_count; }
    [[nodiscard]] double consecutive_diff_max() const { return m_consecutive_diff_count_max; }
    [[nodiscard]] const std::vector<double>& all_diff() const { return m_all_diff_count; }
    [[nodiscard]] double all_diff_max() const { return m_all_diff_count_max; }
    [[nodiscard]] const std::vector<double>& wrapped_timestamp() const { return m_wrapped_timestamp_count; }
    [[nodiscard]] double wrapped_timestamp_max() const { return m_wrapped_timestamp_count_max; }

    // Most recent frequency-domain snapshot; may be null before the first
    // calculation completes.
    [[nodiscard]] std::shared_ptr<const FreqSnapshot> freq_snapshot() const;

    // The highest frequency of the spectrum, in Hz.
    [[nodiscard]] double freq_max_hz() const { return m_bin_rate / 2.0; }

    [[nodiscard]] std::size_t event_count() const { return m_event_count; }
    [[nodiscard]] bool calculating() const;

    // Stops the worker thread.
    void terminate();

private:
    void worker_loop();
    void recompute();

    int m_bin_rate = 16000;
    std::vector<double> m_timestamp_raw;
    std::vector<double> m_timestamp_binned;

    std::vector<double> m_consecutive_diff_count;
    std::vector<double> m_all_diff_count;
    std::vector<double> m_wrapped_timestamp_count;
    double m_consecutive_diff_count_max = 0.0;
    double m_all_diff_count_max = 0.0;
    double m_wrapped_timestamp_count_max = 0.0;

    std::size_t m_event_count = 0;

    // Worker thread state.
    std::thread m_thread;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;
    bool m_dirty = false;
    bool m_calculating = false;
    std::uint64_t m_generation = 0;
    std::uint64_t m_pending_generation = 0;
    std::vector<double> m_pending_consecutive;
    std::vector<double> m_pending_all;
    std::vector<double> m_pending_wrapped;
    std::shared_ptr<const FreqSnapshot> m_snapshot;
};

} // namespace kbi
