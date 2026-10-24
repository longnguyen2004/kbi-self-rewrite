#pragma once

#include <optional>
#include <string>

namespace Kbi::App {

// Opens the native file dialog to pick a recording. Returns nothing when the
// dialog was cancelled.
[[nodiscard]] std::optional<std::string> openRecordingDialog(const std::string& startDirectory);

} // namespace Kbi::app
