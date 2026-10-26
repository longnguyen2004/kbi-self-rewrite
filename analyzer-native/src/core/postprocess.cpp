#include "core/postprocess.h"

#include <algorithm>
#include <cmath>

namespace Kbi {

PostprocessResult postprocessWithMax(const std::vector<double>& arr, PostprocessOptions options)
{
    PostprocessResult result;
    result.data = arr;

    if (options.lowCut) {
        const auto limit = static_cast<int>(std::min<std::size_t>(70, result.data.size()));
        for (int i = 0; i < limit; ++i) {
            const double x = static_cast<double>(i);
            result.data[static_cast<std::size_t>(i)] /= 1.0 + std::exp(-(x - 25.0) / 4.0);
        }
    }

    for (const double value : result.data)
        result.max = std::max(result.max, value);

    return result;
}

} // namespace Kbi
