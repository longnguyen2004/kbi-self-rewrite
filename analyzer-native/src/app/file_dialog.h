#pragma once

#include <optional>
#include <string>

namespace kbi::app {

// Opens the native file dialog to pick a recording. Returns nothing when the
// dialog was cancelled.
[[nodiscard]] std::optional<std::string> open_recording_dialog(
    const std::string& start_directory);

} // namespace kbi::app
