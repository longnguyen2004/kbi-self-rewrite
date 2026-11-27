#include <boost/ut.hpp>

#include <cmath>
#include <core/rolling_mean.h>
#include <print>

int main()
{
    using namespace boost::ut;

    "rolling mean"_test = [] {
        Kbi::RollingMean rolling;
        expect(rolling.empty()) << "a new rolling mean has no samples";

        rolling.add(10.0);
        expect(rolling.mean() == 10.0) << "a single sample is the mean";
        rolling.add(20.0);
        expect(rolling.mean() == 15.0) << "the mean covers the samples so far";

        // Only the last 100 samples are kept: after adding 1 through 101 the
        // window holds 2 through 101, whose mean is 51.5.
        for (int i = 1; i <= 101; ++i)
            rolling.add(static_cast<double>(i));
        expect(rolling.mean() == 51.5) << "the window keeps the last 100 samples";

        rolling.reset();
        expect(rolling.empty()) << "reset drops the samples";
    };
}
