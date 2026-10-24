#pragma once

#include <string>

struct SDL_Renderer;

namespace Kbi::App {

// Captures the current frame of the renderer and writes it as a PPM image.
// Returns false when the capture failed.
bool saveScreenshot(SDL_Renderer& renderer, const std::string& path);

} // namespace Kbi::App
