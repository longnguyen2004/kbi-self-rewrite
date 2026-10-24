#include "ui/charts_common.h"
#include "ui/charts.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <format>

namespace Kbi::Ui {

void showTooltip(const ImVec2& anchor, const std::vector<std::string>& lines)
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

    ImGui::SetNextWindowPos(ImVec2(left + size.x * 0.5f, top + size.y), ImGuiCond_Always,
        ImVec2(0.5f, 1.0f));
    if (ImGui::BeginTooltip()) {
        for (const std::string& line : lines)
            ImGui::TextUnformatted(line.c_str());
        ImGui::EndTooltip();
    }
}

PlotArea currentPlotArea(double xMin, double xMax)
{
    PlotArea area;
    area.position = ImPlot::GetPlotPos();
    area.size = ImPlot::GetPlotSize();

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 min = ImVec2(area.position.x - kHoverMargin, area.position.y - kHoverMargin);
    const ImVec2 max = ImVec2(area.position.x + area.size.x + kHoverMargin,
        area.position.y + area.size.y + kHoverMargin);
    area.hovered = ImGui::IsMouseHoveringRect(min, max) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    if (area.size.x > 1.0f) {
        const double t = (mouse.x - area.position.x) / area.size.x;
        area.mouseX = xMin + t * (xMax - xMin);
    } else {
        area.mouseX = xMin;
    }
    return area;
}

double toPixelX(const PlotArea& area, double xMin, double xMax, double x)
{
    const double span = xMax - xMin;
    if (span <= 0.0)
        return area.position.x;
    return area.position.x + (x - xMin) / span * area.size.x;
}

double toPixelY(const PlotArea& area, double yMin, double yMax, double y)
{
    const double span = yMax - yMin;
    if (span <= 0.0)
        return area.position.y + area.size.y;
    return area.position.y + area.size.y * (1.0 - (y - yMin) / span);
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

    updateRange(sync, interaction);
}

void updateRange(ChartSync& sync, const ChartInteraction& interaction)
{
    const double span = sync.xMax - sync.xMin;
    const double extentSpan = std::max(sync.extentMax - sync.extentMin, 1e-12);
    const double minSpan = std::max(extentSpan * 1e-9, 1e-12);

    if (interaction.dragging && interaction.plotWidth > 1.0f) {
        const double delta = -static_cast<double>(interaction.dragDeltaX) * span / interaction.plotWidth;
        double newMin = sync.xMin + delta;
        double newMax = sync.xMax + delta;
        if (newMin < sync.extentMin) {
            newMin = sync.extentMin;
            newMax = newMin + span;
        }
        if (newMax > sync.extentMax) {
            newMax = sync.extentMax;
            newMin = newMax - span;
        }
        sync.xMin = newMin;
        sync.xMax = newMax;
    }

    if (interaction.wheel != 0.0f) {
        const double factor = std::pow(0.9, static_cast<double>(interaction.wheel));
        const double anchor = std::clamp(interaction.mouseX, sync.xMin, sync.xMax);
        double newMin = anchor - (anchor - sync.xMin) * factor;
        double newMax = anchor + (sync.xMax - anchor) * factor;

        const double newSpan = newMax - newMin;
        if (newSpan < minSpan) {
            const double center = (newMin + newMax) * 0.5;
            newMin = center - minSpan * 0.5;
            newMax = center + minSpan * 0.5;
        }
        if (newSpan > extentSpan) {
            newMin = sync.extentMin;
            newMax = sync.extentMax;
        } else {
            if (newMin < sync.extentMin) {
                newMin = sync.extentMin;
                newMax = newMin + newSpan;
            }
            if (newMax > sync.extentMax) {
                newMax = sync.extentMax;
                newMin = newMax - newSpan;
            }
        }
        sync.xMin = newMin;
        sync.xMax = newMax;
    }

    if (interaction.doubleClicked) {
        sync.xMin = sync.extentMin;
        sync.xMax = sync.extentMax;
    }
}

} // namespace Kbi::ui
