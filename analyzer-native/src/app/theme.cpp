#include "app/theme.h"

#include <imgui.h>
#include <implot.h>

namespace kbi::ui {
namespace {

Theme g_theme = Theme::Dark;

} // namespace

void apply_theme(Theme theme) {
    g_theme = theme;

    if (theme == Theme::Dark) {
        ImGui::StyleColorsDark();
        ImPlot::StyleColorsDark();
    } else {
        ImGui::StyleColorsLight();
        ImPlot::StyleColorsLight();
    }

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);

    ImPlotStyle& plot_style = ImPlot::GetStyle();
    plot_style.PlotPadding = ImVec2(6.0f, 4.0f);
    plot_style.LegendPadding = ImVec2(6.0f, 6.0f);
}

Theme current_theme() {
    return g_theme;
}

void toggle_theme() {
    apply_theme(g_theme == Theme::Dark ? Theme::Light : Theme::Dark);
}

const char* theme_name(Theme theme) {
    return theme == Theme::Dark ? "Dark" : "Light";
}

} // namespace kbi::ui
