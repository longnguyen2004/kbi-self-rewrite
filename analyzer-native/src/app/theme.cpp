#include "app/theme.h"

#include <imgui.h>
#include <implot.h>

namespace Kbi::Ui {
namespace {

Theme gTheme = Theme::Dark;

} // namespace

void applyTheme(Theme theme)
{
    gTheme = theme;

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

    ImPlotStyle& plotStyle = ImPlot::GetStyle();
    plotStyle.PlotPadding = ImVec2(6.0f, 4.0f);
    plotStyle.LegendPadding = ImVec2(6.0f, 6.0f);
}

Theme currentTheme()
{
    return gTheme;
}

void toggleTheme()
{
    applyTheme(gTheme == Theme::Dark ? Theme::Light : Theme::Dark);
}

const char* themeName(Theme theme)
{
    return theme == Theme::Dark ? "Dark" : "Light";
}

} // namespace Kbi::ui
