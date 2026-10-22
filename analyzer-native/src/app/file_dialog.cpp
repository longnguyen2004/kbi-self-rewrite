#include "app/file_dialog.h"

#include <tinyfiledialogs/tinyfiledialogs.h>

#include <array>
#include <iterator>

namespace Kbi::App {

std::optional<std::string> openRecordingDialog(const std::string& startDirectory)
{
    static constexpr std::array<const char*, 2> patterns = { "*.json", "*.kbi" };

    // TinyFileDialogs only treats the default path as a directory when it
    // ends with a separator.
    std::string defaultPath = startDirectory;
    if (!defaultPath.empty() && defaultPath.back() != '/')
        defaultPath += '/';

    const char* selected = tinyfd_openFileDialog("Open recording", defaultPath.c_str(),
        static_cast<int>(patterns.size()), patterns.data(),
        "recording files", 0);
    if ((selected == nullptr) || ((*selected) == 0))
        return std::nullopt;

    return std::string(selected);
}

} // namespace Kbi::app
