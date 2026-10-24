#pragma once

#include "core/input_timeline.h"
#include "ui/point_snapper.h"

#include <imgui.h>
#include <implot.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace Kbi::Ui {

// Forward declaration, the full definition lives in charts.h.
struct ChartSync;

// Extra room around the plot area that still counts as "over the chart".
inline constexpr float kHoverMargin = 16.0f;

// The radius of the hit test around the cursor, in pixels.
inline constexpr double kTooltipRadius = 40.0;
// The margin between the bars of the input timeline and the edges of their
// row, in pixels.
inline constexpr double kBarMargin = 3.0;
// The distance of the tooltip above the point that it describes.
inline constexpr float kTooltipOffset = 12.0f;

// The area of the plot, and whether the mouse is over it.
struct PlotArea {
    ImVec2 position { 0.0f, 0.0f };
    ImVec2 size { 0.0f, 0.0f };
    bool hovered = false;
    double mouseX = 0.0; // mouse position in data units
};

PlotArea currentPlotArea(double xMin, double xMax);
double toPixelX(const PlotArea& area, double xMin, double xMax, double x);
double toPixelY(const PlotArea& area, double yMin, double yMax, double y);

void showTooltip(const ImVec2& anchor, const std::vector<std::string>& lines);
void handleInteraction(ChartSync& sync, const PlotArea& area);

} // namespace Kbi::Ui
