#include <tests/tests.h>

#include <core/postprocess.h>
#include <print>
#include <vector>

namespace Kbi::Tests {
void testPostprocess()
{
    std::println("postprocess");
    const std::vector<double> data(100, 1.0);
    Kbi::PostprocessResult result;
    Kbi::postprocessWithMax(data, { true }, result);
    check(result.data[0] < 0.01, "the low cut removes the dc component");
    check(result.data[99] == 1.0, "the low cut leaves the high bins alone");
    check(result.max == 1.0, "the max is computed");

    Kbi::postprocessWithMax(data, { false }, result);
    check(result.data[0] == 1.0, "the low cut can be disabled");
}

} // namespace Kbi::Tests
