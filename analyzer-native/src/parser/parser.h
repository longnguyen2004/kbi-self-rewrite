#pragma once

#include "core/result.h"

#include <optional>
#include <string>

namespace kbi {

// Parses the JSON result format produced by the recorder's console mode.
[[nodiscard]] std::optional<Result> parse_result_json(const std::string& json_text);

// Parses a recording from a file. Dispatches on the file contents: JSON
// results and legacy binary `.kbi` recordings are both supported.
[[nodiscard]] std::optional<Result> parse_result_file(const std::string& path);

// Reads a whole file into memory.
[[nodiscard]] std::optional<std::string> read_file(const std::string& path);

} // namespace kbi
