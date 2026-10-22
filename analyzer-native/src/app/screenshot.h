#pragma once

#include <string>

namespace Kbi::App {

// Captures the current framebuffer and writes it as a PPM image.
// Returns false when the capture failed.
bool saveScreenshot(const std::string& path);

} // namespace Kbi::app
