#pragma once

#include <optional>
#include <string>

namespace Kbi::App {

// Opens the native file dialog to pick a recording. Returns nothing when the
// dialog was cancelled.
[[nodiscard]] std::optional<std::string> openRecordingDialog(const std::string& startDirectory);

#if !defined(__EMSCRIPTEN__)
// Opens the native file dialog to pick where a recording is saved. Returns
// nothing when the dialog was cancelled. Recording is native only.
[[nodiscard]] std::optional<std::string> saveRecordingDialog(const std::string& startDirectory);
#endif

} // namespace Kbi::App
