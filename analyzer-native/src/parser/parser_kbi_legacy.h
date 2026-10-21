#pragma once

#include "core/result.h"

#include <optional>
#include <vector>

namespace kbi {

// Parses the legacy binary `.kbi` format (the MATLAB "Keyboard Builder
// Instrument" layout, version 3). Returns nothing if the buffer is not a
// supported KBI file.
[[nodiscard]] std::optional<Result> parse_kbi_legacy(const std::vector<unsigned char>& buffer);

} // namespace kbi
