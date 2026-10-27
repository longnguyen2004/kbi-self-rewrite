#include <app/app.h>
#include <app/screenshot.h>
#include <app/theme.h>
#include <ui/log_store.h>

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <implot.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <SDL3/SDL.h>

#include <boost/di.hpp>
#include <boost/program_options.hpp>

#include <cstdio>
#include <format>
#include <iostream>
#include <optional>
#include <print>
#include <string>

namespace di = boost::di;
namespace po = boost::program_options;

namespace {

// The frame at which the --screenshot capture is taken.
constexpr int kScreenshotFrame = 30;

// How long to sleep per iteration while the window is minimized, in
// milliseconds.
constexpr Uint32 kMinimizedDelayMs = 10;

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

// Creates the renderer of the window; prints the error and returns nothing
// on failure.
SDL_Renderer* createRenderer(SDL_Window* window)
{
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr)
        std::println(stderr, "Failed to create the SDL renderer: {}", SDL_GetError());
    return renderer;
}

} // namespace

int main(int argc, char** argv)
{
    std::optional<Arguments> arguments;
    try {
        arguments = parseArguments(argc, argv);
    } catch (const std::exception& e) {
        std::println(stderr, "Error: {}", e.what());
        std::println(stderr, "Try analyzer-native --help");
        return 1;
    }
    if (!arguments)
        return 0;

    // The dependencies are wired together with boost::di: the logger writes
    // to the terminal and into the log store, which is a singleton shared
    // with the "Logs" dialog of the app. The recorder gets the logger passed
    // by the app, then it reports in both.
    const auto injector = di::make_injector(di::bind<Kbi::Ui::LogStore>.in(di::singleton),
        di::bind<spdlog::logger>.to([](const auto& injector) -> std::shared_ptr<spdlog::logger> {
            auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            consoleSink->set_level(spdlog::level::info);
            auto logStore = injector.template create<std::shared_ptr<Kbi::Ui::LogStore>>();
            return std::make_shared<spdlog::logger>(
                "kbi", spdlog::sinks_init_list { consoleSink, logStore });
        }));

    // Wayland compositors work in scaled points, and when the fractional
    // scaling handshake does not complete they upscale the backbuffer from
    // its logical size, which makes the whole program blurry. Working in
    // pixel coordinates keeps the backbuffer 1:1 with the screen pixels on
    // every compositor, rendering the UI at the same size as the X11 driver.
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_SCALE_TO_DISPLAY, "1");
    if (!SDL_SetAppMetadata("Keyboard Inspector", "1.0.0", "com.keyboard-inspector.analyzer")) {
        std::println(stderr, "Failed to set the SDL app metadata: {}", SDL_GetError());
        return 1;
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::println(stderr, "Failed to initialize SDL: {}", SDL_GetError());
        return 1;
    }

    constexpr int width = 1600;
    constexpr int height = 900;
    SDL_Window* window = SDL_CreateWindow(
        "Keyboard Inspector", width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        std::println(stderr, "Failed to create the SDL window: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = createRenderer(window);
    if (renderer == nullptr) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImPlot::CreateContext();
    Kbi::Ui::applyTheme(Kbi::Ui::Theme::Dark);

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    auto app = injector.create<std::unique_ptr<Kbi::App::App>>();
    if (!arguments->openPath.empty())
        app->load(arguments->openPath);
    if (arguments->record)
        app->startRecording();
    if (arguments->showInfo)
        app->setInfoVisible(true);

    int frame = 0;
    bool done = false;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
                && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }
        if ((SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) != 0) {
            SDL_Delay(kMinimizedDelayMs);
            continue;
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // The SDL renderer backend samples every texture with nearest
        // filtering unless a draw callback of the frame asks for linear
        // sampling. The chart lines are anti-aliased through a pre-rendered
        // texture that needs linear sampling, so request it before anything
        // else is drawn.
        ImGui::GetBackgroundDrawList()->AddCallback(
            ImGui::GetPlatformIO().DrawCallback_SetSamplerLinear, nullptr);

        app->draw();

        ImGui::Render();

        // The SDL renderer works in logical points, scale the draw data into
        // the window's pixel density.
        const ImGuiIO& io = ImGui::GetIO();
        SDL_SetRenderScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

        SDL_SetRenderDrawColorFloat(renderer, 0.06f, 0.06f, 0.07f, 1.0f);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);

        if (ImGui::IsKeyPressed(ImGuiKey_F12))
            Kbi::App::saveScreenshot(*renderer, "screenshot.ppm");

        ++frame;
        if (!arguments->screenshotPath.empty() && frame >= arguments->screenshotFrame) {
            if (!Kbi::App::saveScreenshot(*renderer, arguments->screenshotPath))
                std::println(stderr, "Failed to write {}", arguments->screenshotPath);
            done = true;
        }

        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
