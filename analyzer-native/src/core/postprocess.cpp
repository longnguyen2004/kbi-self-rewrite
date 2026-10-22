#include "core/postprocess.h"

#include <algorithm>
#include <cmath>

namespace Kbi {

PostprocessResult postprocessWithMax(const std::vector<double>& arr,
    const PostprocessOptions& options)
{
    PostprocessResult result;
    result.data = arr;

    if (options.lowCut) {
        const std::size_t limit = std::min<std::size_t>(70, result.data.size());
        for (std::size_t i = 0; i < limit; ++i) {
            const double x = static_cast<double>(i);
            result.data[i] /= 1.0 + std::exp(-(x - 25.0) / 4.0);
        }
    }

    for (const double value : result.data)
        result.max = std::max(result.max, value);

    return result;
}

} // namespace Kbi
