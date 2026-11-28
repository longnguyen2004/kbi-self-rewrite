#include <app/save_recording.h>

#include <exporter.h>
#include <serializer.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

#if defined(__linux__)
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <sys/fsuid.h>
#endif

namespace Kbi::App {
namespace {

std::filesystem::path withExtension(const std::filesystem::path& path, std::string_view extension)
{
    std::filesystem::path result = path;
    result.replace_extension(extension);
    return result;
}

} // namespace

std::optional<std::string> saveRecordingToFile(const Recorder& recorder, const std::string& path)
{
    const std::filesystem::path jsonPath = withExtension(path, ".json");
    const std::filesystem::path kbiPath = withExtension(path, ".kbi");

#if defined(__linux__)
    // Like the CLI recorder, write as the invoking user when the app was
    // started with sudo. Restore the effective uid once both files are done.
    uid_t uid = getuid();
    if (const char* sudoUid = std::getenv("SUDO_UID"))
        std::from_chars(sudoUid, sudoUid + std::strlen(sudoUid), uid);
    setfsuid(uid);
#endif

    std::optional<std::string> error;

    if (std::fstream out(jsonPath, std::ios::out | std::ios::binary | std::ios::trunc); out) {
        const JsonTextSerializer serializer;
        serializer.serialize(recorder, out);
        if (!out)
            error = std::format("Failed to write {}", jsonPath.string());
    } else
        error = std::format("Failed to open {}", jsonPath.string());

    if (std::fstream out(kbiPath, std::ios::out | std::ios::binary | std::ios::trunc); out) {
        const ExporterMatKbi exporter(recorder);
        exporter.exportTo(out);
        if (!out && !error)
            error = std::format("Failed to write {}", kbiPath.string());
    } else if (!error)
        error = std::format("Failed to open {}", kbiPath.string());

#if defined(__linux__)
    setfsuid(geteuid());
#endif

    return error;
}

} // namespace Kbi::App
