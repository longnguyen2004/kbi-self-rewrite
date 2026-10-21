#include "core/analyzer.h"

#include "core/fft.h"

#include <algorithm>
#include <cmath>

namespace kbi {
namespace {

FreqSnapshot compute_snapshot(const std::vector<double>& consecutive,
                               const std::vector<double>& all,
                               const std::vector<double>& wrapped) {
    FreqSnapshot snapshot;

    const fft::RealSpectrum consecutive_spectrum = fft::real_dft(consecutive);
    const fft::RealSpectrum all_spectrum = fft::real_dft(all);
    const fft::RealSpectrum wrapped_spectrum = fft::real_dft(wrapped);

    // The length of the histogram, the spectrum is normalized by it.
    const std::size_t len = consecutive.size();
    // The number of bins of the spectrum.
    const std::size_t bins = consecutive_spectrum.values.size() / 2;
    if (bins == 0)
        return snapshot;

    snapshot.consecutive_diff.resize(bins);
    snapshot.all_diff.resize(bins);
    snapshot.wrapped_timestamp.resize(bins);

    for (std::size_t freq = 0; freq < bins; ++freq) {
        // The first and the last bin are not mirrored, so they are not doubled.
        const double norm = ((freq == 0 || freq + 1 == bins) ? 1.0 : 2.0) / static_cast<double>(len);
        const double consecutive_mag =
            std::hypot(consecutive_spectrum.values[freq * 2],
                       consecutive_spectrum.values[freq * 2 + 1]) *
            norm;
        const double all_mag =
            std::hypot(all_spectrum.values[freq * 2], all_spectrum.values[freq * 2 + 1]) * norm;
        const double wrapped_mag =
            std::hypot(wrapped_spectrum.values[freq * 2], wrapped_spectrum.values[freq * 2 + 1]) *
            norm;

        snapshot.consecutive_diff[freq] = consecutive_mag;
        snapshot.all_diff[freq] = all_mag;
        snapshot.wrapped_timestamp[freq] = wrapped_mag;

        snapshot.consecutive_diff_max = std::max(snapshot.consecutive_diff_max, consecutive_mag);
        snapshot.all_diff_max = std::max(snapshot.all_diff_max, all_mag);
        snapshot.wrapped_timestamp_max = std::max(snapshot.wrapped_timestamp_max, wrapped_mag);
    }

    return snapshot;
}

} // namespace

Analyzer::Analyzer() {
    reset();
    m_thread = std::thread([this] { worker_loop(); });
}

Analyzer::~Analyzer() {
    terminate();
}

void Analyzer::terminate() {
    {
        std::lock_guard lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

void Analyzer::reset() {
    ++m_generation;
    m_timestamp_raw.clear();
    m_timestamp_binned.clear();
    m_consecutive_diff_count.assign(static_cast<std::size_t>(m_bin_rate) + 1, 0.0);
    m_all_diff_count.assign(static_cast<std::size_t>(m_bin_rate) + 1, 0.0);
    m_wrapped_timestamp_count.assign(static_cast<std::size_t>(m_bin_rate) + 1, 0.0);
    m_consecutive_diff_count_max = 0.0;
    m_all_diff_count_max = 0.0;
    m_wrapped_timestamp_count_max = 0.0;
    m_event_count = 0;

    std::lock_guard lock(m_mutex);
    m_pending_consecutive.clear();
    m_pending_all.clear();
    m_pending_wrapped.clear();
    m_snapshot.reset();
}

void Analyzer::set_bin_rate(int bin_rate) {
    if (bin_rate <= 0)
        return;
    if (bin_rate % 125 != 0)
        return;
    const int v = bin_rate / 125;
    if ((v & (v - 1)) != 0)
        return;
    if (bin_rate == m_bin_rate)
        return;

    m_bin_rate = bin_rate;
    const std::vector<double> old_timestamps = m_timestamp_raw;
    reset();
    add(old_timestamps);
}

void Analyzer::add(const std::vector<double>& timestamps) {
    if (timestamps.empty())
        return;

    const double interval = 1000000.0 / static_cast<double>(m_bin_rate);
    std::size_t added = 0;

    for (const double timestamp : timestamps) {
        // Timestamps have to be monotonically increasing.
        if (!m_timestamp_raw.empty() && timestamp < m_timestamp_raw.back())
            continue;
        m_timestamp_raw.push_back(timestamp);

        const double binned = std::round(timestamp / interval) * interval;

        if (!m_timestamp_binned.empty()) {
            const double last = m_timestamp_binned.back();
            if (last != 0.0) {
                // Consecutive diff: gap to the previous event.
                const double consecutive_diff = binned - last;
                if (consecutive_diff <= 1000000.0) {
                    const auto index = static_cast<std::size_t>(
                        std::llround(consecutive_diff / interval));
                    if (index < m_consecutive_diff_count.size()) {
                        m_consecutive_diff_count[index] += 1.0;
                        m_consecutive_diff_count_max =
                            std::max(m_consecutive_diff_count_max, m_consecutive_diff_count[index]);
                    }
                }

                // All diff: gap to every previous event within the last second.
                const auto start = std::lower_bound(m_timestamp_binned.begin(),
                                                    m_timestamp_binned.end(),
                                                    binned - 1000000.0);
                for (auto i = start; i != m_timestamp_binned.end(); ++i) {
                    const double long_diff = binned - *i;
                    const auto index = static_cast<std::size_t>(std::llround(long_diff / interval));
                    if (index < m_all_diff_count.size()) {
                        m_all_diff_count[index] += 1.0;
                        m_all_diff_count_max =
                            std::max(m_all_diff_count_max, m_all_diff_count[index]);
                    }
                }

                // Fractional part of the timestamp within a one second window.
                const double binned_wrapped = std::fmod(binned, 1000000.0);
                const auto wrapped_index =
                    static_cast<std::size_t>(std::llround(binned_wrapped / interval));
                if (wrapped_index < m_wrapped_timestamp_count.size()) {
                    m_wrapped_timestamp_count[wrapped_index] += 1.0;
                    m_wrapped_timestamp_count_max =
                        std::max(m_wrapped_timestamp_count_max, m_wrapped_timestamp_count[wrapped_index]);
                }
            }
        }

        m_timestamp_binned.push_back(binned);
        ++added;
    }

    m_event_count += added;

    {
        std::lock_guard lock(m_mutex);
        m_pending_consecutive = m_consecutive_diff_count;
        m_pending_all = m_all_diff_count;
        m_pending_wrapped = m_wrapped_timestamp_count;
        m_pending_generation = m_generation;
        m_dirty = true;
    }
    m_cv.notify_one();
}

std::shared_ptr<const FreqSnapshot> Analyzer::freq_snapshot() const {
    std::lock_guard lock(m_mutex);
    return m_snapshot;
}

bool Analyzer::calculating() const {
    std::lock_guard lock(m_mutex);
    return m_dirty || m_calculating;
}

void Analyzer::worker_loop() {
    for (;;) {
        std::vector<double> consecutive;
        std::vector<double> all;
        std::vector<double> wrapped;
        std::uint64_t generation = 0;
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [this] { return m_stop || m_dirty; });
            if (m_stop)
                return;
            consecutive = std::move(m_pending_consecutive);
            all = std::move(m_pending_all);
            wrapped = std::move(m_pending_wrapped);
            m_pending_consecutive.clear();
            m_pending_all.clear();
            m_pending_wrapped.clear();
            generation = m_pending_generation;
            m_dirty = false;
            m_calculating = true;
        }

        auto snapshot = std::make_shared<FreqSnapshot>(
            compute_snapshot(consecutive, all, wrapped));

        {
            std::lock_guard lock(m_mutex);
            // The analyzer may have been reset while the snapshot was being
            // computed, in that case the result is stale and must be dropped.
            if (generation == m_generation)
                m_snapshot = snapshot;
            m_calculating = false;
        }
    }
}

} // namespace kbi
