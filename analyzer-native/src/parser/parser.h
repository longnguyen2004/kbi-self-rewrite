#pragma once

#include <core/result.h>

#include <optional>
#include <string>

namespace Kbi {

// Parses the JSON result format produced by the recorder's console mode.
[[nodiscard]] std::optional<Result> parseResultJson(const std::string& jsonText);

// Parses a recording from a file. Dispatches on the file contents: JSON
// results and legacy binary `.kbi` recordings are both supported.
[[nodiscard]] std::optional<Result> parseResultFile(const std::string& path);

// Reads a whole file into memory.
[[nodiscard]] std::optional<std::string> readFile(const std::string& path);

} // namespace Kbi
