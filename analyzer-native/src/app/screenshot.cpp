#include "app/screenshot.h"

// Use the same OpenGL loader as the ImGui backend, so the calls go through
// the same dispatch table as the rendering.
#include "imgui_impl_opengl3_loader.h"

#include <cstdio>
#include <format>
#include <print>
#include <fstream>
#include <vector>

// The ImGui loader only declares the enums it needs.
#ifndef GL_RGB
#define GL_RGB 0x1907
#endif
#ifndef GL_PACK_ALIGNMENT
#define GL_PACK_ALIGNMENT 0x0D05
#endif

namespace kbi::app {

bool save_screenshot(const std::string& path) {
    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int width = viewport[2];
    const int height = viewport[3];
    if (width <= 0 || height <= 0) {
        std::println(stderr, "Invalid viewport {}x{}", width, height);
        return false;
    }

    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) *
                                      static_cast<std::size_t>(height) * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    std::ofstream stream(path, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!stream) {
        std::println(stderr, "Could not open {} for writing", path);
        return false;
    }

    stream << "P6\n" << width << " " << height << "\n255\n";
    // GL reads bottom-up, write the rows top-down.
    for (int y = height - 1; y >= 0; --y) {
        const unsigned char* row = pixels.data() + static_cast<std::size_t>(y) *
                                                        static_cast<std::size_t>(width) * 3;
        stream.write(reinterpret_cast<const char*>(row), static_cast<std::streamsize>(width) * 3);
    }
    return true;
}

} // namespace kbi::app
