#include "app/app.h"
#include "app/screenshot.h"
#include "app/theme.h"
#include "ui/log_store.h"

#include <boost/di.hpp>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// Use the same OpenGL loader as the ImGui backend so that every GL call goes
// through the same dispatch table.
#include "imgui_impl_opengl3_loader.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cstdio>
#include <format>
#include <print>
#include <string>

namespace di = boost::di;

namespace {

void glfwErrorCallback(int error, const char* description)
{
    std::println(stderr, "GLFW error {}: {}", error, description);
}

} // namespace

int main(int argc, char** argv)
{
    std::string screenshotPath;
    std::string openPath;
    bool record = false;
    bool showInfo = false;
    int screenshotFrame = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--screenshot" && i + 1 < argc) {
            screenshotPath = argv[++i];
            screenshotFrame = 30;
        } else if (argument == "--open" && i + 1 < argc) {
            openPath = argv[++i];
        } else if (argument == "--record") {
            record = true;
        } else if (argument == "--info") {
            showInfo = true;
        } else if (argument == "--help") {
            std::println("Usage: analyzer-native [--open <recording>] [--record] [--info] "
                         "[--screenshot <file.ppm>]");
            return 0;
        }
    }

    // The dependencies are wired together with boost::di: the logger writes
    // to the terminal and into the log store, which is a singleton shared
    // with the "Logs" dialog of the app. The recorder gets the logger passed
    // by the app, then it reports in both.
    const auto injector = di::make_injector(
        di::bind<Kbi::Ui::LogStore>.in(di::singleton),
        di::bind<spdlog::logger>.to([](const auto& injector) -> std::shared_ptr<spdlog::logger> {
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_level(spdlog::level::info);
            auto logStore = injector.template create<std::shared_ptr<Kbi::Ui::LogStore>>();
            return std::make_shared<spdlog::logger>("kbi",
                spdlog::sinks_init_list { consoleSink, logStore });
        }));

    glfwSetErrorCallback(glfwErrorCallback);
    if (glfwInit() == 0) {
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
    if (window == nullptr) {
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
    Kbi::Ui::applyTheme(Kbi::Ui::Theme::Dark);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    auto app = injector.create<std::unique_ptr<Kbi::App::App>>();
    if (!openPath.empty())
        app->load(openPath);
    if (record)
        app->startRecording();
    if (showInfo)
        app->setInfoVisible(true);

    int frame = 0;
    while (glfwWindowShouldClose(window) == 0) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        app->draw();

        ImGui::Render();
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (ImGui::IsKeyPressed(ImGuiKey_F12))
            Kbi::App::saveScreenshot("screenshot.ppm");

        ++frame;
        if (!screenshotPath.empty() && frame >= screenshotFrame) {
            if (!Kbi::App::saveScreenshot(screenshotPath))
                std::println(stderr, "Failed to write {}", screenshotPath);
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
