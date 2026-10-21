#include "app/app.h"
#include "app/screenshot.h"
#include "app/theme.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>

// Use the same OpenGL loader as the ImGui backend so that every GL call goes
// through the same dispatch table.
#include "imgui_impl_opengl3_loader.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdio>
#include <format>
#include <print>
#include <string>

namespace {

void glfw_error_callback(int error, const char* description) {
    std::println(stderr, "GLFW error {}: {}", error, description);
}

} // namespace

int main(int argc, char** argv) {
    std::string screenshot_path;
    std::string open_path;
    bool record = false;
    bool show_info = false;
    int screenshot_frame = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--screenshot" && i + 1 < argc) {
            screenshot_path = argv[++i];
            screenshot_frame = 30;
        } else if (argument == "--open" && i + 1 < argc) {
            open_path = argv[++i];
        } else if (argument == "--record") {
            record = true;
        } else if (argument == "--info") {
            show_info = true;
        } else if (argument == "--help") {
            std::println("Usage: analyzer-native [--open <recording>] [--record] [--info] "
                         "[--screenshot <file.ppm>]");
            return 0;
        }
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) {
        std::println(stderr, "Failed to initialize GLFW");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif

    const int width = 1600;
    const int height = 900;
    GLFWwindow* window = glfwCreateWindow(width, height, "Keyboard Inspector", nullptr, nullptr);
    if (!window) {
        std::println(stderr, "Failed to create the GLFW window");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImPlot::CreateContext();
    kbi::ui::apply_theme(kbi::ui::Theme::Dark);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    kbi::app::App app;
    if (!open_path.empty())
        app.load(open_path);
    if (record)
        app.start_recording();
    if (show_info)
        app.set_info_visible(true);

    int frame = 0;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app.draw();

        ImGui::Render();
        int framebuffer_width = 0;
        int framebuffer_height = 0;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        glViewport(0, 0, framebuffer_width, framebuffer_height);
        glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (ImGui::IsKeyPressed(ImGuiKey_F12))
            kbi::app::save_screenshot("screenshot.ppm");

        ++frame;
        if (!screenshot_path.empty() && frame >= screenshot_frame) {
            if (!kbi::app::save_screenshot(screenshot_path))
                std::println(stderr, "Failed to write {}", screenshot_path);
            break;
        }

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
