#include "core/analyzer.h"

#include "core/fft.h"

#include <algorithm>
#include <cmath>

namespace Kbi {
namespace {

FreqSnapshot computeSnapshot(const std::vector<double>& consecutive, const std::vector<double>& all,
    const std::vector<double>& wrapped)
{
    FreqSnapshot snapshot;

    const Fft::RealSpectrum consecutiveSpectrum = Fft::realDft(consecutive);
    const Fft::RealSpectrum allSpectrum = Fft::realDft(all);
    const Fft::RealSpectrum wrappedSpectrum = Fft::realDft(wrapped);

    // The length of the histogram, the spectrum is normalized by it.
    const std::size_t len = consecutive.size();
    // The number of bins of the spectrum.
    const std::size_t bins = consecutiveSpectrum.values.size() / 2;
    if (bins == 0)
        return snapshot;

    snapshot.consecutiveDiff.resize(bins);
    snapshot.allDiff.resize(bins);
    snapshot.wrappedTimestamp.resize(bins);

    for (std::size_t freq = 0; freq < bins; ++freq) {
        // The first and the last bin are not mirrored, so they are not doubled.
        const double norm
            = ((freq == 0 || freq + 1 == bins) ? 1.0 : 2.0) / static_cast<double>(len);
        const double consecutiveMag = std::hypot(consecutiveSpectrum.values[freq * 2],
                                          consecutiveSpectrum.values[freq * 2 + 1])
            * norm;
        const double allMag
            = std::hypot(allSpectrum.values[freq * 2], allSpectrum.values[freq * 2 + 1]) * norm;
        const double wrappedMag
            = std::hypot(wrappedSpectrum.values[freq * 2], wrappedSpectrum.values[freq * 2 + 1])
            * norm;

        snapshot.consecutiveDiff[freq] = consecutiveMag;
        snapshot.allDiff[freq] = allMag;
        snapshot.wrappedTimestamp[freq] = wrappedMag;

        snapshot.consecutiveDiffMax = std::max(snapshot.consecutiveDiffMax, consecutiveMag);
        snapshot.allDiffMax = std::max(snapshot.allDiffMax, allMag);
        snapshot.wrappedTimestampMax = std::max(snapshot.wrappedTimestampMax, wrappedMag);
    }

    return snapshot;
}

} // namespace

Analyzer::Analyzer()
{
    reset();
    m_thread = std::thread([this] { workerLoop(); });
}

Analyzer::~Analyzer() { terminate(); }

void Analyzer::terminate()
{
    {
        std::scoped_lock lock(m_mutex);
        m_stop = true;
    }
    m_cv.notify_all();
    if (m_thread.joinable())
        m_thread.join();
}

void Analyzer::reset()
{
    ++m_generation;
    m_timestampRaw.clear();
    m_timestampBinned.clear();
    // The histograms have one entry per hertz of the spectrum, their length is
    // the bin rate. It is also the size of the FFT, which PFFFT supports.
    m_consecutiveDiffCount.assign(static_cast<std::size_t>(m_binRate), 0.0);
    m_allDiffCount.assign(static_cast<std::size_t>(m_binRate), 0.0);
    m_wrappedTimestampCount.assign(static_cast<std::size_t>(m_binRate), 0.0);
    m_consecutiveDiffCountMax = 0.0;
    m_allDiffCountMax = 0.0;
    m_wrappedTimestampCountMax = 0.0;
    m_eventCount = 0;

    std::scoped_lock lock(m_mutex);
    m_pendingConsecutive.clear();
    m_pendingAll.clear();
    m_pendingWrapped.clear();
    m_snapshot.reset();
}

void Analyzer::setBinRate(int binRate)
{
    if (binRate <= 0)
        return;
    if (binRate % 125 != 0)
        return;
    const int v = binRate / 125;
    if ((v & (v - 1)) != 0)
        return;
    if (binRate == m_binRate)
        return;

    m_binRate = binRate;
    const std::vector<double> oldTimestamps = m_timestampRaw;
    reset();
    add(oldTimestamps);
}

void Analyzer::add(const std::vector<double>& timestamps)
{
    if (timestamps.empty())
        return;

    const double interval = 1000000.0 / static_cast<double>(m_binRate);
    std::size_t added = 0;

    for (const double timestamp : timestamps) {
        // Timestamps have to be monotonically increasing.
        if (!m_timestampRaw.empty() && timestamp < m_timestampRaw.back())
            continue;
        m_timestampRaw.push_back(timestamp);

        const double binned = std::round(timestamp / interval) * interval;

        if (!m_timestampBinned.empty()) {
            const double last = m_timestampBinned.back();
            if (last != 0.0) {
                // Consecutive diff: gap to the previous event.
                const double consecutiveDiff = binned - last;
                if (consecutiveDiff < 1000000.0) {
                    const auto index
                        = static_cast<std::size_t>(std::llround(consecutiveDiff / interval));
                    if (index < m_consecutiveDiffCount.size()) {
                        m_consecutiveDiffCount[index] += 1.0;
                        m_consecutiveDiffCountMax
                            = std::max(m_consecutiveDiffCountMax, m_consecutiveDiffCount[index]);
                    }
                }

                // All diff: gap to every previous event within the last second.
                const auto start = std::lower_bound(
                    m_timestampBinned.begin(), m_timestampBinned.end(), binned - 1000000.0);
                for (auto i = start; i != m_timestampBinned.end(); ++i) {
                    const double longDiff = binned - *i;
                    const auto index = static_cast<std::size_t>(std::llround(longDiff / interval));
                    if (index < m_allDiffCount.size()) {
                        m_allDiffCount[index] += 1.0;
                        m_allDiffCountMax = std::max(m_allDiffCountMax, m_allDiffCount[index]);
                    }
                }

                // Fractional part of the timestamp within a one second window.
                const double binnedWrapped = std::fmod(binned, 1000000.0);
                const auto wrappedIndex
                    = static_cast<std::size_t>(std::llround(binnedWrapped / interval));
                if (wrappedIndex < m_wrappedTimestampCount.size()) {
                    m_wrappedTimestampCount[wrappedIndex] += 1.0;
                    m_wrappedTimestampCountMax = std::max(
                        m_wrappedTimestampCountMax, m_wrappedTimestampCount[wrappedIndex]);
                }
            }
        }

        m_timestampBinned.push_back(binned);
        ++added;
    }

    m_eventCount += added;

    {
        std::scoped_lock lock(m_mutex);
        m_pendingConsecutive = m_consecutiveDiffCount;
        m_pendingAll = m_allDiffCount;
        m_pendingWrapped = m_wrappedTimestampCount;
        m_pendingGeneration = m_generation;
        m_dirty = true;
    }
    m_cv.notify_one();
}

std::shared_ptr<const FreqSnapshot> Analyzer::freqSnapshot() const
{
    std::scoped_lock lock(m_mutex);
    return m_snapshot;
}

bool Analyzer::calculating() const
{
    std::scoped_lock lock(m_mutex);
    return m_dirty || m_calculating;
}

void Analyzer::workerLoop()
{
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
            consecutive = std::move(m_pendingConsecutive);
            all = std::move(m_pendingAll);
            wrapped = std::move(m_pendingWrapped);
            m_pendingConsecutive.clear();
            m_pendingAll.clear();
            m_pendingWrapped.clear();
            generation = m_pendingGeneration;
            m_dirty = false;
            m_calculating = true;
        }

        auto snapshot = std::make_shared<FreqSnapshot>(computeSnapshot(consecutive, all, wrapped));

        {
            std::scoped_lock lock(m_mutex);
            // The analyzer may have been reset while the snapshot was being
            // computed, in that case the result is stale and must be dropped.
            if (generation == m_generation)
                m_snapshot = snapshot;
            m_calculating = false;
        }
    }
}

} // namespace Kbi
