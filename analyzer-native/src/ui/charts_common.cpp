#include <ui/charts.h>
#include <ui/charts_common.h>

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace Kbi::Ui {

void showTooltip(ImVec2 anchor, const std::vector<std::string>& lines)
{
    // The size of the tooltip is estimated, so that it can be kept inside the
    // viewport. The web analyzer does the same with its tooltip placement.
    ImVec2 size(0.0f, 0.0f);
    for (const std::string& line : lines) {
        const ImVec2 lineSize = ImGui::CalcTextSize(line.c_str());
        size.x = std::max(size.x, lineSize.x);
        size.y += lineSize.y;
    }
    const ImGuiStyle& style = ImGui::GetStyle();
    size.x += style.WindowPadding.x * 2.0f;
    size.y += style.WindowPadding.y * 2.0f;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float left = std::clamp(anchor.x - size.x * 0.5f, viewport->WorkPos.x,
        viewport->WorkPos.x + viewport->WorkSize.x - size.x);
    const float top = std::clamp(anchor.y - kTooltipOffset - size.y, viewport->WorkPos.y,
        viewport->WorkPos.y + viewport->WorkSize.y - size.y);

    ImGui::SetNextWindowPos(
        ImVec2(left + size.x * 0.5f, top + size.y), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    if (ImGui::BeginTooltip()) {
        for (const std::string& line : lines)
            ImGui::TextUnformatted(line.c_str());
        ImGui::EndTooltip();
    }
}

PlotArea currentPlotArea(AxisRange domain)
{
    PlotArea area;
    area.position = ImPlot::GetPlotPos();
    area.size = ImPlot::GetPlotSize();

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 min = ImVec2(area.position.x - kHoverMargin, area.position.y - kHoverMargin);
    const ImVec2 max = ImVec2(
        area.position.x + area.size.x + kHoverMargin, area.position.y + area.size.y + kHoverMargin);
    area.hovered = ImGui::IsMouseHoveringRect(min, max)
        && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    if (area.size.x > 1.0f) {
        const double t = (mouse.x - area.position.x) / area.size.x;
        area.mouseX = domain.min + t * (domain.max - domain.min);
    } else {
        area.mouseX = domain.min;
    }
    return area;
}

double toPixelX(const PlotArea& area, AxisRange domain, double x)
{
    const double span = domain.max - domain.min;
    if (span <= 0.0)
        return area.position.x;
    return area.position.x + (x - domain.min) / span * area.size.x;
}

double toPixelY(const PlotArea& area, AxisRange domain, double y)
{
    const double span = domain.max - domain.min;
    if (span <= 0.0)
        return area.position.y + area.size.y;
    return area.position.y + area.size.y * (1.0 - (y - domain.min) / span);
}

void handleInteraction(ChartSync& sync, const PlotArea& area)
{
    if (!area.hovered)
        return;

    ChartInteraction interaction;
    interaction.dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    interaction.dragDeltaX = ImGui::GetIO().MouseDelta.x;
    interaction.wheel = ImGui::GetIO().MouseWheel;
    interaction.mouseX = area.mouseX;
    interaction.doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    interaction.plotWidth = area.size.x;

    sync.updateRange(interaction);
}

void ChartSync::updateRange(const ChartInteraction& interaction)
{
    const double span = xMax - xMin;
    const double extentSpan = std::max(extentMax - extentMin, 1e-12);
    const double minSpan = std::max(extentSpan * 1e-9, 1e-12);

    if (interaction.dragging && interaction.plotWidth > 1.0f) {
        const double delta
            = -static_cast<double>(interaction.dragDeltaX) * span / interaction.plotWidth;
        double newMin = xMin + delta;
        double newMax = xMax + delta;
        if (newMin < extentMin) {
            newMin = extentMin;
            newMax = newMin + span;
        }
        if (newMax > extentMax) {
            newMax = extentMax;
            newMin = newMax - span;
        }
        xMin = newMin;
        xMax = newMax;
    }

    if (interaction.wheel != 0.0f) {
        const double factor = std::pow(0.9, static_cast<double>(interaction.wheel));
        const double anchor = std::clamp(interaction.mouseX, xMin, xMax);
        double newMin = anchor - (anchor - xMin) * factor;
        double newMax = anchor + (xMax - anchor) * factor;

        const double newSpan = newMax - newMin;
        if (newSpan < minSpan) {
            const double center = (newMin + newMax) * 0.5;
            newMin = center - minSpan * 0.5;
            newMax = center + minSpan * 0.5;
        }
        if (newSpan > extentSpan) {
            newMin = extentMin;
            newMax = extentMax;
        } else {
            if (newMin < extentMin) {
                newMin = extentMin;
                newMax = newMin + newSpan;
            }
            if (newMax > extentMax) {
                newMax = extentMax;
                newMin = newMax - newSpan;
            }
        }
        xMin = newMin;
        xMax = newMax;
    }

    if (interaction.doubleClicked) {
        xMin = extentMin;
        xMax = extentMax;
    }
}

} // namespace Kbi::Ui
