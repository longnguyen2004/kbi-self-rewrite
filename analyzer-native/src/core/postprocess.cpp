#include <core/postprocess.h>

#include <algorithm>
#include <cmath>

namespace Kbi {

void postprocessWithMax(
    const std::vector<double>& arr, PostprocessOptions options, PostprocessResult& out)
{
    out.data = arr;
    out.max = 0.0;

    if (options.lowCut) {
        const auto limit = static_cast<int>(std::min<std::size_t>(70, out.data.size()));
        for (int i = 0; i < limit; ++i) {
            const double x = static_cast<double>(i);
            out.data[static_cast<std::size_t>(i)] /= 1.0 + std::exp(-(x - 25.0) / 4.0);
        }
    }

    for (const double value : out.data)
        out.max = std::max(out.max, value);
}

} // namespace Kbi
