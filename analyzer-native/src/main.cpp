#include "app/app.h"
#include "app/screenshot.h"
#include "app/theme.h"
#include "ui/log_store.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <implot.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <SDL3/SDL.h>

#include <boost/di.hpp>

#include <cstdio>
#include <format>
#include <print>
#include <string>

namespace di = boost::di;

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

    const int width = 1600;
    const int height = 900;
    SDL_Window* window = SDL_CreateWindow(
        "Keyboard Inspector", width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        std::println(stderr, "Failed to create the SDL window: {}", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        std::println(stderr, "Failed to create the SDL renderer: {}", SDL_GetError());
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
    if (!openPath.empty())
        app->load(openPath);
    if (record)
        app->startRecording();
    if (showInfo)
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
            SDL_Delay(10);
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
        if (!screenshotPath.empty() && frame >= screenshotFrame) {
            if (!Kbi::App::saveScreenshot(*renderer, screenshotPath))
                std::println(stderr, "Failed to write {}", screenshotPath);
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
