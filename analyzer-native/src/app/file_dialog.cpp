#include <app/file_dialog.h>

#if defined(__EMSCRIPTEN__)
#include <array>
#include <optional>
#include <string>
#else
#include <tinyfiledialogs/tinyfiledialogs.h>

#include <array>
#include <optional>
#include <string>
#endif

namespace Kbi::App {

#if defined(__EMSCRIPTEN__)
// Implemented by the JS shim in web/file_dialog.js: it shows a browser file
// picker, copies the selected file into the in-memory filesystem and writes
// its path into `pathBuffer`. Returns 0 when the dialog was cancelled.
extern "C" int kbi_open_recording_dialog(
    char* pathBuffer, int pathBufferSize, const char* startDirectory);
#endif

std::optional<std::string> openRecordingDialog(const std::string& startDirectory)
{
#if defined(__EMSCRIPTEN__)
    std::array<char, 4096> path {};
    const int result = kbi_open_recording_dialog(
        path.data(), static_cast<int>(path.size()), startDirectory.c_str());
    if (result == 0)
        return std::nullopt;
    return std::string { path.data() };
#else
    static constexpr std::array<const char*, 2> patterns = { "*.json", "*.kbi" };

    // TinyFileDialogs only treats the default path as a directory when it
    // ends with a separator.
    std::string defaultPath = startDirectory;
    if (!defaultPath.empty() && defaultPath.back() != '/')
        defaultPath += '/';

    const char* selected = tinyfd_openFileDialog("Open recording", defaultPath.c_str(),
        static_cast<int>(patterns.size()), patterns.data(), "recording files", 0);
    if ((selected == nullptr) || ((*selected) == 0))
        return std::nullopt;

    return std::string(selected);
#endif
}

} // namespace Kbi::App
