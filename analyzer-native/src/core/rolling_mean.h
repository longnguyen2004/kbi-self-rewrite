#pragma once

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/rolling_mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

#include <cstddef>

namespace Kbi {

// A moving average over the last 100 samples, computed with
// boost::accumulators::rolling_mean.
class RollingMean {
public:
    RollingMean()
        : m_samples(boost::accumulators::rolling_window_size = kWindow)
    {
    }

    void add(double value) { m_samples(value); }

    [[nodiscard]] bool empty() const { return boost::accumulators::count(m_samples) == 0; }

    [[nodiscard]] double mean() const { return boost::accumulators::rolling_mean(m_samples); }

    void reset() { m_samples = Accumulator { boost::accumulators::rolling_window_size = kWindow }; }

private:
    static constexpr std::size_t kWindow = 100;
    using Accumulator = boost::accumulators::accumulator_set<double,
        boost::accumulators::stats<boost::accumulators::tag::rolling_mean,
            boost::accumulators::tag::count>>;
    Accumulator m_samples;
};

} // namespace Kbi
