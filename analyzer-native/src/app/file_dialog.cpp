#include "app/file_dialog.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_dialog.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string>

namespace Kbi::App {

namespace {

struct DialogState {
    std::atomic<bool> done { false };
    std::atomic<std::shared_ptr<std::string>> result {};
};

void SDLCALL onFileSelected(void* userdata, const char* const* filelist, int /*filter*/)
{
    auto* state = static_cast<DialogState*>(userdata);
    if (filelist != nullptr && filelist[0] != nullptr && filelist[0][0] != '\0')
        state->result.store(std::make_shared<std::string>(filelist[0]), std::memory_order_release);
    state->done.store(true, std::memory_order_release);
}

} // namespace

std::optional<std::string> openRecordingDialog(const std::string& startDirectory)
{
    static const SDL_DialogFileFilter filters[] = {
        { "recording files", "json;kbi" },
    };

    DialogState state;

    const char* defaultLocation = startDirectory.empty() ? nullptr : startDirectory.c_str();
    SDL_ShowOpenFileDialog(onFileSelected, &state, nullptr, filters, 1, defaultLocation, false);

    while (!state.done.load(std::memory_order_acquire)) {
        SDL_PumpEvents();
        SDL_Delay(5);
    }

    if (const auto result = state.result.load(std::memory_order_acquire))
        return *result;
    return std::nullopt;
}

} // namespace Kbi::app