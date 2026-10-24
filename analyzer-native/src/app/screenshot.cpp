#include "app/screenshot.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <format>
#include <fstream>
#include <print>
#include <string>

namespace Kbi::App {

bool saveScreenshot(SDL_Renderer& renderer, const std::string& path)
{
    // The read covers the whole output in pixels, so undo the DPI scale of
    // the drawing first. The rendered frame itself is not affected.
    SDL_SetRenderScale(&renderer, 1.0f, 1.0f);
    SDL_Surface* raw = SDL_RenderReadPixels(&renderer, nullptr);
    if (raw == nullptr) {
        std::println(stderr, "Failed to read the frame: {} (renderer: {})", SDL_GetError(),
            SDL_GetRendererName(&renderer));
        return false;
    }

    // The PPM format stores plain RGB, the renderer hands out whatever pixel
    // format its backbuffer has.
    SDL_Surface* surface = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_RGB24);
    SDL_DestroySurface(raw);
    if (surface == nullptr) {
        std::println(stderr, "Failed to convert the frame: {}", SDL_GetError());
        return false;
    }

    std::ofstream stream(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream) {
        std::println(stderr, "Could not open {} for writing", path);
        SDL_DestroySurface(surface);
        return false;
    }

    stream << "P6\n" << surface->w << " " << surface->h << "\n255\n";
    // SDL hands the rows over top-down, no flip is needed.
    for (int y = 0; y < surface->h; ++y) {
        const auto* row = static_cast<const char*>(surface->pixels)
            + static_cast<std::ptrdiff_t>(y) * surface->pitch;
        stream.write(row, static_cast<std::streamsize>(surface->w) * 3);
    }
    SDL_DestroySurface(surface);
    return true;
}

} // namespace Kbi::App
