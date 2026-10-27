#include <app/theme.h>

#include <imgui.h>
#include <implot.h>

namespace Kbi::Ui {
namespace {

// The current theme lives in a function-local static instead of a
// namespace-scope variable (I.2, R.6).
Theme& activeTheme()
{
    static Theme theme = Theme::Dark;
    return theme;
}

} // namespace

void applyTheme(Theme theme)
{
    activeTheme() = theme;

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

Theme currentTheme() { return activeTheme(); }

void toggleTheme() { applyTheme(activeTheme() == Theme::Dark ? Theme::Light : Theme::Dark); }

std::string_view themeName(Theme theme) { return theme == Theme::Dark ? "Dark" : "Light"; }

} // namespace Kbi::Ui
