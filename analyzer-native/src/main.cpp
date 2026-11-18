#include <app/app.h>
#include <app/screenshot.h>
#include <app/theme.h>
#include <ui/log_store.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// Use the same OpenGL loader as the ImGui backend so that every GL call goes
// through the same dispatch table. Emscripten provides GLES directly.
#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#else
#include "imgui_impl_opengl3_loader.h"
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <boost/di.hpp>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#else
#include <boost/program_options.hpp>
#endif

#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <string>

namespace di = boost::di;
#if !defined(__EMSCRIPTEN__)
namespace po = boost::program_options;
#endif

namespace {

// The frame at which the --screenshot capture is taken.
constexpr int kScreenshotFrame = 30;

void glfwErrorCallback(int error, const char* description)
{
    std::println(stderr, "GLFW error {}: {}", error, description);
}

} // namespace

namespace {

// The command line options of the analyzer.
struct Arguments {
    std::string screenshotPath;
    std::string openPath;
    bool record = false;
    bool showInfo = false;
    int screenshotFrame = 0;
};

#if !defined(__EMSCRIPTEN__)
// Parses the command line with boost::program_options. Returns nothing when
// the program should exit immediately because the usage message was printed
// (F.3 — argument handling kept out of main).
std::optional<Arguments> parseArguments(int argc, char** argv)
{
    Arguments arguments;

    po::options_description description("Options");
    description.add_options()("help", "Show this help and exit")(
        "record", po::bool_switch(&arguments.record), "Start a recording at startup")(
        "info", po::bool_switch(&arguments.showInfo), "Show the info window at startup")("open",
        po::value<std::string>(&arguments.openPath),
        "Load a recording at startup (.json or legacy .kbi file)")("screenshot",
        po::value<std::string>(&arguments.screenshotPath),
        "Save a screenshot to the given .ppm file some frames after startup");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, description), vm);
    if (vm.contains("help")) {
        std::cout << description << "\n";
        return std::nullopt;
    }
    // Recognised values are written into the Arguments fields, unknown or
    // malformed options throw.
    po::notify(vm);
    if (vm.contains("screenshot"))
        arguments.screenshotFrame = kScreenshotFrame;
    return arguments;
}
#endif

// Everything the frame function needs. The emscripten main loop only passes
// a single pointer around, so the state is kept in one place that both the
// native and the browser build can use.
struct Session {
    GLFWwindow* window = nullptr;
    std::unique_ptr<Kbi::App::App> app;
    std::string screenshotPath;
    int screenshotFrame = 0;
    int frame = 0;
};

// Draws one frame. Returns false when the window was closed.
bool runFrame(Session& session)
{
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    session.app->draw();

    ImGui::Render();

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(session.window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glClearColor(0.06f, 0.06f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (ImGui::IsKeyPressed(ImGuiKey_F12))
        Kbi::App::saveScreenshot("screenshot.ppm");

    ++session.frame;
    bool done = false;
    if (!session.screenshotPath.empty() && session.frame >= session.screenshotFrame) {
        if (!Kbi::App::saveScreenshot(session.screenshotPath))
            std::println(stderr, "Failed to write {}", session.screenshotPath);
        done = true;
    }

    glfwSwapBuffers(session.window);
    return !done && glfwWindowShouldClose(session.window) == 0;
}

} // namespace

#if defined(__EMSCRIPTEN__)
namespace {

// The session has to outlive main(): the browser main loop keeps running
// after the wasm entry point returned.
std::unique_ptr<Session> gSession;

void webFrame(void* userData)
{
    auto& session = *static_cast<Session*>(userData);
    if (!runFrame(session))
        emscripten_cancel_main_loop();
}

} // namespace
#endif

int main(int argc, char** argv)
{
    std::optional<Arguments> arguments;
#if defined(__EMSCRIPTEN__)
    arguments = Arguments {};
#else
    try {
        arguments = parseArguments(argc, argv);
    } catch (const std::exception& e) {
        std::println(stderr, "Error: {}", e.what());
        std::println(stderr, "Try analyzer-native --help");
        return 1;
    }
#endif
    if (!arguments)
        return 0;

    // The dependencies are wired together with boost::di: the logger writes
    // to the terminal and into the log store, which is a singleton shared
    // with the "Logs" dialog of the app.
    const auto injector = di::make_injector(di::bind<Kbi::Ui::LogStore>.in(di::singleton),
        di::bind<spdlog::logger>.to([](const auto& injector) -> std::shared_ptr<spdlog::logger> {
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_level(spdlog::level::info);
            auto logStore = injector.template create<std::shared_ptr<Kbi::Ui::LogStore>>();
            return std::make_shared<spdlog::logger>(
                "kbi", spdlog::sinks_init_list { consoleSink, logStore });
        }));

    glfwSetErrorCallback(glfwErrorCallback);
    if (glfwInit() == 0) {
        std::println(stderr, "Failed to initialize GLFW");
        return 1;
    }

#if defined(__EMSCRIPTEN__)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    constexpr const char* kGlslVersion = "#version 300 es";
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    constexpr const char* kGlslVersion = "#version 150";
#endif

    constexpr int width = 1600;
    constexpr int height = 900;
    auto session = std::make_unique<Session>();
    session->window = glfwCreateWindow(width, height, "Keyboard Inspector", nullptr, nullptr);
    if (session->window == nullptr) {
        std::println(stderr, "Failed to create the GLFW window");
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(session->window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImPlot::CreateContext();
    Kbi::Ui::applyTheme(Kbi::Ui::Theme::Dark);

    ImGui_ImplGlfw_InitForOpenGL(session->window, true);
#if defined(__EMSCRIPTEN__)
    // Ties the canvas size and the mouse wheel to the browser page.
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(session->window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(kGlslVersion);

    session->app = injector.create<std::unique_ptr<Kbi::App::App>>();
    if (!arguments->openPath.empty())
        session->app->load(arguments->openPath);
#if !defined(__EMSCRIPTEN__)
    if (arguments->record)
        session->app->startRecording();
#endif
    if (arguments->showInfo)
        session->app->setInfoVisible(true);
    session->screenshotPath = arguments->screenshotPath;
    session->screenshotFrame = arguments->screenshotFrame;

#if defined(__EMSCRIPTEN__)
    gSession = std::move(session);
    // The browser cannot block in a while loop, it drives the frame function
    // from requestAnimationFrame; simulating an infinite loop keeps main()
    // from returning and tearing the runtime down.
    emscripten_set_main_loop_arg(webFrame, gSession.get(), 0, true);
#else
    while (runFrame(*session)) {
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(session->window);
    glfwTerminate();
#endif

    return 0;
}
