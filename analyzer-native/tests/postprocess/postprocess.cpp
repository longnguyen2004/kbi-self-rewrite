#include <boost/ut.hpp>

#include <core/postprocess.h>
#include <print>
#include <vector>

int main()
{
    using namespace boost::ut;

    "postprocess"_test = [] {
        const std::vector<double> data(100, 1.0);
        Kbi::PostprocessResult result;
        Kbi::postprocessWithMax(data, { true }, result);
        expect(result.data[0] < 0.01) << "the low cut removes the dc component";
        expect(result.data[99] == 1.0) << "the low cut leaves the high bins alone";
        expect(result.max == 1.0) << "the max is computed";

        Kbi::postprocessWithMax(data, { false }, result);
        expect(result.data[0] == 1.0) << "the low cut can be disabled";
    };
}
