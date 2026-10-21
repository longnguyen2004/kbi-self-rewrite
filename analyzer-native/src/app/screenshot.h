#pragma once

#include <string>

namespace kbi::app {

// Captures the current framebuffer and writes it as a PPM image.
// Returns false when the capture failed.
bool save_screenshot(const std::string& path);

} // namespace kbi::app
