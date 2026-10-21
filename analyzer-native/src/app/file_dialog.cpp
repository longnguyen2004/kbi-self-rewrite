#include "app/file_dialog.h"

#include <tinyfiledialogs/tinyfiledialogs.h>

#include <iterator>

namespace kbi::app {

std::optional<std::string> open_recording_dialog(const std::string& start_directory) {
    static const char* const patterns[] = {"*.json", "*.kbi"};

    // TinyFileDialogs only treats the default path as a directory when it
    // ends with a separator.
    std::string default_path = start_directory;
    if (!default_path.empty() && default_path.back() != '/')
        default_path += '/';

    const char* selected = tinyfd_openFileDialog("Open recording", default_path.c_str(),
                                                  static_cast<int>(std::size(patterns)), patterns,
                                                  "recording files", 0);
    if (!selected || !*selected)
        return std::nullopt;

    return std::string(selected);
}

} // namespace kbi::app
