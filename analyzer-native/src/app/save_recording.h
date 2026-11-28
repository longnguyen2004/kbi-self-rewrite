#pragma once

#include <optional>
#include <string>

namespace Kbi {

class Recorder;

namespace App {

// Writes a recording to disk in both formats produced by the CLI recorder:
// the JSON result and the legacy binary .kbi. The chosen path's extension is
// replaced with .json and .kbi, so the two files end up side by side.
// Returns a human readable error when one of the files cannot be written.
[[nodiscard]] std::optional<std::string> saveRecordingToFile(
    const Recorder& recorder, const std::string& path);

} // namespace App
} // namespace Kbi
