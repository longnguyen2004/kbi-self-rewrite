#include "ui/charts.h"

#include "core/decimate.h"
#include "ui/point_snapper.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <format>
#include <functional>
#include <map>
#include <set>
#include <string_view>

namespace Kbi::Ui {
namespace {

constexpr ImVec4 kDiffColor(65.0f / 255.0f, 140.0f / 255.0f, 240.0f / 255.0f, 1.0f);
constexpr ImVec4 kFreqColor(240.0f / 255.0f, 120.0f / 255.0f, 0.0f, 1.0f);

// Extra room around the plot area that still counts as "over the chart".
constexpr float kHoverMargin = 16.0f;

// The colors assigned to the devices in the order they are seen.
constexpr std::array<ImVec4, 6> kDevicePalette = {
    ImVec4(65.0f / 255.0f, 140.0f / 255.0f, 240.0f / 255.0f, 1.0f),
    ImVec4(240.0f / 255.0f, 120.0f / 255.0f, 0.0f, 1.0f),
    ImVec4(80.0f / 255.0f, 200.0f / 255.0f, 120.0f / 255.0f, 1.0f),
    ImVec4(220.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f, 1.0f),
    ImVec4(180.0f / 255.0f, 100.0f / 255.0f, 220.0f / 255.0f, 1.0f),
    ImVec4(240.0f / 255.0f, 200.0f / 255.0f, 60.0f / 255.0f, 1.0f),
};

// The tick positions of the difference charts: the range is covered with
// steps of 0.125, 0.25, 0.5, 1, 2, 5, ... units.
std::vector<double> millisecondTicks(double min, double max)
{
    const double range = max - min;
    double step = 200.0;
    if (range <= 1.5)
        step = 0.125;
    else if (range <= 3.0)
        step = 0.25;
    else if (range <= 5.0)
        step = 0.5;
    else if (range <= 10.0)
        step = 1.0;
    else if (range <= 20.0)
        step = 2.0;
    else if (range <= 50.0)
        step = 5.0;
    else if (range <= 100.0)
        step = 10.0;
    else if (range <= 200.0)
        step = 20.0;
    else if (range <= 500.0)
        step = 50.0;
    else if (range <= 1000.0)
        step = 100.0;

    const double firstIndex = std::floor(min / step + 1.0);
    const double lastIndex = std::ceil(max / step - 1.0);
    const double quantizedMin = firstIndex * step;
    const std::size_t tickCount = lastIndex < firstIndex ? 0 : static_cast<std::size_t>(std::llround(lastIndex - firstIndex)) + 1;

    std::vector<double> ticks;
    ticks.push_back(min);
    for (std::size_t i = 0; i < tickCount; ++i)
        ticks.push_back(quantizedMin + static_cast<double>(i) * step);
    ticks.push_back(max);
    return ticks;
}

// The tick positions of the frequency charts: 125Hz, 250Hz, 500Hz, ...
std::vector<double> hertzTicks(double min, double max)
{
    const auto steps = static_cast<std::uint64_t>(std::floor((max - min) / 1000.0));
    const double step = std::clamp(125.0 * static_cast<double>(std::bit_ceil(steps)), 125.0, 8000.0);
    const double firstIndex = std::floor(min / step + 1.0);
    const double lastIndex = std::ceil(max / step - 1.0);
    const double quantizedMin = firstIndex * step;
    const std::size_t tickCount = lastIndex < firstIndex ? 0 : static_cast<std::size_t>(std::llround(lastIndex - firstIndex)) + 1;

    std::vector<double> ticks;
    ticks.push_back(min);
    for (std::size_t i = 0; i < tickCount; ++i)
        ticks.push_back(quantizedMin + static_cast<double>(i) * step);
    ticks.push_back(max);
    return ticks;
}

// The radius of the hit test around the cursor, in pixels.
constexpr double kTooltipRadius = 40.0;
// The margin between the bars of the input timeline and the edges of their
// row, in pixels.
constexpr double kBarMargin = 3.0;
// The distance of the tooltip above the point that it describes.
constexpr float kTooltipOffset = 12.0f;

// Shows the tooltip of a data point, anchored above it.
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

// The area of the plot, and whether the mouse is over it.
struct PlotArea {
    ImVec2 position { 0.0f, 0.0f };
    ImVec2 size { 0.0f, 0.0f };
    bool hovered = false;
    double mouseX = 0.0; // mouse position in data units
};

PlotArea currentPlotArea(double xMin, double xMax)
{
    PlotArea area;
    area.position = ImPlot::GetPlotPos();
    area.size = ImPlot::GetPlotSize();

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 min = ImVec2(area.position.x - kHoverMargin, area.position.y - kHoverMargin);
    const ImVec2 max = ImVec2(area.position.x + area.size.x + kHoverMargin,
        area.position.y + area.size.y + kHoverMargin);
    // A window that is on top of the chart (the info popup for example) takes
    // the hover away from it, the chart must not react to the mouse then.
    area.hovered = ImGui::IsMouseHoveringRect(min, max) && ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    if (area.size.x > 1.0f) {
        const double t = (mouse.x - area.position.x) / area.size.x;
        area.mouseX = xMin + t * (xMax - xMin);
    } else {
        area.mouseX = xMin;
    }
    return area;
}

// Converts a data position into a pixel position inside the plot.
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

// Handles panning, zooming and resetting. The range is stored in the sync
// object and applied on the next frame.
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

} // namespace

void updateRange(ChartSync& sync, const ChartInteraction& interaction)
{
    const double span = sync.xMax - sync.xMin;
    const double extentSpan = std::max(sync.extentMax - sync.extentMin, 1e-12);
    const double minSpan = std::max(extentSpan * 1e-9, 1e-12);

    if (interaction.dragging && interaction.plotWidth > 1.0f) {
        const double delta = -static_cast<double>(interaction.dragDeltaX) * span / interaction.plotWidth;
        double newMin = sync.xMin + delta;
        double newMax = sync.xMax + delta;
        // Keep the view inside the data extent.
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
        // The cursor is the anchor of the zoom. When it is outside of the
        // visible range it is clamped to the closest edge, otherwise the range
        // would collapse.
        const double anchor = std::clamp(interaction.mouseX, sync.xMin, sync.xMax);
        double newMin = anchor - (anchor - sync.xMin) * factor;
        double newMax = anchor + (sync.xMax - anchor) * factor;

        const double newSpan = newMax - newMin;
        if (newSpan < minSpan) {
            const double center = (newMin + newMax) * 0.5;
            newMin = center - minSpan * 0.5;
            newMax = center + minSpan * 0.5;
        }
        // The view stays inside the extent of the data.
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

void ChartRenderer::drawLineChart(std::string_view id,
    std::string_view title,
    const std::vector<double>& data,
    double yMax,
    ChartSync& sync,
    const ImVec4& color,
    std::string_view xLabel,
    std::string_view yLabel,
    double initialSpan,
    TickStyle tickStyle,
    TooltipStyle tooltipStyle)
{
    const double extentMin = sync.extentMin;
    const double extentMax = sync.extentMax;
    const double extentSpan = std::max(extentMax - extentMin, 1e-12);

    if (!sync.initialized || sync.dataLength != data.size()) {
        sync.initialized = true;
        sync.dataLength = data.size();
        sync.xMin = extentMin;
        sync.xMax = extentMin + std::min(initialSpan, extentSpan);
    }
    sync.xMin = std::clamp(sync.xMin, extentMin, extentMax);
    sync.xMax = std::clamp(sync.xMax, extentMin, extentMax);
    if (sync.xMax <= sync.xMin)
        sync.xMax = std::min(extentMax, sync.xMin + std::max(extentSpan * 1e-9, 1e-12));

    // ImGui and ImPlot take C strings. The ids, titles and axis labels are
    // always string literals, so they are guaranteed to be NUL-terminated.
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    ImGui::PushID(id.data());
    if (!title.empty())
        // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
        ImGui::TextUnformatted(title.data());

    // ImPlot's own input handling is disabled, the interactions below are
    // implemented explicitly so that they behave the same in every chart.
    if (!ImPlot::BeginPlot("##plot", ImVec2(-1.0f, m_chartHeight),
            ImPlotFlags_NoTitle | ImPlotFlags_NoInputs)) {
        ImPlot::EndPlot();
        ImGui::PopID();
        return;
    }

    // The y axis goes from zero to the maximum of the data, like in the web
    // analyzer. Only an empty chart falls back to a range of one.
    const double yRange = yMax > 0.0 ? yMax : 1.0;

    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    ImPlot::SetupAxis(ImAxis_X1, xLabel.data());
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    ImPlot::SetupAxis(ImAxis_Y1, yLabel.data());
    ImPlot::SetupAxisLimits(ImAxis_X1, sync.xMin, sync.xMax, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, yRange, ImPlotCond_Always);

    // Custom ticks, generated the same way as in the web analyzer.
    if (tickStyle != TickStyle::Automatic) {
        const std::vector<double> generated = tickStyle == TickStyle::Milliseconds
            ? millisecondTicks(sync.xMin, sync.xMax)
            : hertzTicks(sync.xMin, sync.xMax);
        std::vector<double> values;
        std::vector<std::string> labelStorage;
        for (const double value : generated) {
            std::string label;
            if (tickStyle == TickStyle::Milliseconds) {
                const double fractional = value - std::floor(value);
                // Only multiples of an eighth are labelled.
                if (std::floor(fractional * 8.0) - fractional * 8.0 != 0.0)
                    continue;
                if (fractional == 0.0)
                    label = std::format("{}ms", value);
                else
                    label = std::format("{:.3f}", fractional + 1.0);
            } else {
                if (value != std::floor(value))
                    continue;
                label = std::format("{}Hz", value);
            }
            values.push_back(value);
            labelStorage.push_back(std::move(label));
        }
        std::vector<const char*> labels;
        labels.reserve(labelStorage.size());
        for (const std::string& label : labelStorage)
            labels.push_back(label.c_str());
        ImPlot::SetupAxisTicks(ImAxis_X1, values.data(), static_cast<int>(values.size()),
            labels.data());
    }

    const PlotArea area = currentPlotArea(sync.xMin, sync.xMax);
    const int width = static_cast<int>(area.size.x);

    std::vector<ChartPoint> points;
    if (!data.empty() && width > 0)
        points = decimate(data, extentMin, extentMax, sync.xMin, sync.xMax, width);

    if (!points.empty()) {
        std::vector<double> xs;
        std::vector<double> ys;
        xs.reserve(points.size());
        ys.reserve(points.size());
        for (const ChartPoint& point : points) {
            xs.push_back(point.x);
            ys.push_back(point.y);
        }
        ImPlotSpec spec;
        spec.LineColor = color;
        spec.LineWeight = 1.0f;
        spec.Flags = static_cast<ImPlotItemFlags>(ImPlotItemFlags_NoLegend);
        ImPlot::PlotLine("##series", xs.data(), ys.data(), static_cast<int>(xs.size()), spec);
    }

    handleInteraction(sync, area);

    // The pixel positions of the data points, used by the tooltip.
    std::vector<SnapPoint> snapPoints;
    snapPoints.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        snapPoints.push_back(SnapPoint { .x = toPixelX(area, sync.xMin, sync.xMax, points[i].x),
            .y = toPixelY(area, 0.0, yRange, points[i].y),
            .index = static_cast<int>(i) });
    }
    m_snapper.set(std::move(snapPoints));

    ImPlot::EndPlot();

    // The tooltip of the point under the cursor.
    if (area.hovered) {
        const ImVec2 mouse = ImGui::GetMousePos();
        if (const SnapPoint* snapped = m_snapper.find(mouse.x, mouse.y, kTooltipRadius)) {
            const ChartPoint& point = points[static_cast<std::size_t>(snapped->index)];
            std::vector<std::string> lines;
            if (tooltipStyle == TooltipStyle::Difference) {
                lines.push_back(std::format("x: {:.3f}ms", point.x));
                lines.push_back(std::format("y: {} events", point.y));
            } else {
                lines.push_back(std::format("freq: {}Hz", std::lround(point.x)));
                lines.push_back(std::format("mag: {:.3f}", point.y));
            }
            showTooltip(mouse, lines);
        }
    }

    ImGui::PopID();
}

void ChartRenderer::drawDiffChart(std::string_view id,
    std::string_view title,
    const std::vector<double>& data,
    double yMax,
    ChartSync& sync)
{
    // The histograms cover a one second window, expressed in milliseconds.
    sync.extentMin = 0.0;
    sync.extentMax = 1000.0;
    drawLineChart(id, title, data, yMax, sync, kDiffColor, "ms", "events", 25.0,
        TickStyle::Milliseconds, TooltipStyle::Difference);
}

void ChartRenderer::drawFreqChart(std::string_view id,
    std::string_view title,
    const std::vector<double>& data,
    double yMax,
    double maxFrequency,
    ChartSync& sync)
{
    sync.extentMin = 0.0;
    sync.extentMax = maxFrequency;
    drawLineChart(id, title, data, yMax, sync, kFreqColor, "Hz", "magnitude", maxFrequency,
        TickStyle::Hertz, TooltipStyle::Frequency);
}

void ChartRenderer::resetZoom()
{
    m_timelineSync = ChartSync {};
}

void ChartRenderer::setTimelineRange(double xMin, double xMax)
{
    m_timelineSync.initialized = true;
    m_timelineSync.xMin = xMin;
    m_timelineSync.xMax = xMax;
    // The range was set explicitly, it does not follow the end of a recording.
    m_timelineSync.followEnd = false;
}

void ChartRenderer::drawInputTimeline(std::string_view id,
    const InputTimeline& timeline,
    const std::vector<std::string>& deviceIds)
{
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    ImGui::PushID(id.data());

    const std::vector<std::string>& keySet = timeline.keys();
    const std::vector<Keypress>& keypresses = timeline.keypresses();
    const double keyCount = static_cast<double>(keySet.size());

    // Map every device to a colour, in the order the devices were seen.
    std::map<std::string, ImU32> deviceColors;
    for (std::size_t i = 0; i < deviceIds.size(); ++i)
        deviceColors[deviceIds[i]] = ImGui::ColorConvertFloat4ToU32(kDevicePalette[i % kDevicePalette.size()]);

    // The index of every key on the y axis.
    std::map<std::string, double> keyIndices;
    {
        double index = 0.0;
        for (const std::string& key : keySet)
            keyIndices.emplace(key, index++);
    }

    // The keypresses that are drawn, they are the ones the tooltip snaps to.
    std::vector<std::reference_wrapper<const Keypress>> drawn;
    drawn.reserve(keypresses.size());

    for (const Keypress& keypress : keypresses) {
        if (!keyIndices.contains(keypress.key))
            continue;
        drawn.emplace_back(keypress);
    }

    const double endTimestamp = timeline.endTimestamp();

    ChartSync& sync = m_timelineSync;
    const double previousExtentMax = sync.extentMax;
    sync.extentMin = 0.0;
    sync.extentMax = std::max(endTimestamp, 1.0);
    if (!sync.initialized) {
        sync.initialized = true;
        sync.xMin = sync.extentMin;
        sync.xMax = sync.extentMax;
        // A live recording follows its end.
        sync.followEnd = true;
    } else if (sync.followEnd && sync.xMax >= previousExtentMax - 1e-9) {
        // The view was at the right edge, follow the end of the recording.
        sync.xMax = sync.extentMax;
    }
    sync.xMax = std::clamp(sync.xMax, sync.extentMin, sync.extentMax);

    // The timeline takes up all the space that is left.
    if (!ImPlot::BeginPlot("##plot", ImVec2(-1.0f, -1.0f),
            ImPlotFlags_NoTitle | ImPlotFlags_NoInputs)) {
        ImPlot::EndPlot();
        ImGui::PopID();
        return;
    }

    ImPlot::SetupAxis(ImAxis_X1, "s");
    ImPlot::SetupAxisLimits(ImAxis_X1, sync.xMin, sync.xMax, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, -0.5,
        std::max(static_cast<double>(keySet.size()) - 0.5, 0.5),
        ImPlotCond_Always);

    if (!keySet.empty()) {
        std::vector<double> tickValues;
        std::vector<const char*> tickLabels;
        tickValues.reserve(keySet.size());
        tickLabels.reserve(keySet.size());
        for (const std::string& key : keySet) {
            tickValues.push_back(static_cast<double>(tickValues.size()));
            tickLabels.push_back(key.c_str());
        }
        ImPlot::SetupAxisTicks(ImAxis_Y1, tickValues.data(), static_cast<int>(tickValues.size()),
            tickLabels.data());
    }

    const PlotArea area = currentPlotArea(sync.xMin, sync.xMax);

    handleInteraction(sync, area);

    ImPlot::EndPlot();

    // The bars fill the row of their key, with a small margin on both sides.
    // They are drawn on the draw list, ImPlot's line items cannot be made
    // that thick.
    if (!drawn.empty()) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 plotMin = area.position;
        const ImVec2 plotMax(area.position.x + area.size.x, area.position.y + area.size.y);
        drawList->PushClipRect(plotMin, plotMax, true);

        const double rowHeight = keyCount > 0.0 ? area.size.y / keyCount : 0.0;
        const double margin = std::min(kBarMargin, rowHeight / 4.0);

        for (const Keypress& keypress : drawn) {
            const double x0 = toPixelX(area, sync.xMin, sync.xMax, keypress.start);
            const double x1 = toPixelX(area, sync.xMin, sync.xMax, keypress.end);
            // Cull the bars that are outside of the view.
            if (x1 < plotMin.x || x0 > plotMax.x)
                continue;

            const auto index = keyIndices.find(keypress.key);
            if (index == keyIndices.end())
                continue;

            const double yTop = toPixelY(area, -0.5, keyCount - 0.5, index->second + 0.5);
            const double yBottom = toPixelY(area, -0.5, keyCount - 0.5, index->second - 0.5);

            const ImVec2 pMin(static_cast<float>(x0), static_cast<float>(yTop + margin));
            const ImVec2 pMax(static_cast<float>(std::max(x1, x0 + 1.0)),
                static_cast<float>(std::max(yBottom - margin, yTop + margin + 1.0)));

            const ImU32 color = deviceColors.contains(keypress.deviceId)
                ? deviceColors[keypress.deviceId]
                : IM_COL32(200, 200, 200, 255);
            drawList->AddRectFilled(pMin, pMax, color);
        }

        drawList->PopClipRect();
    }

    // The tooltip of the keypress under the cursor. A bar is hit when the
    // cursor lies inside its rectangle, which is more precise than snapping.
    if (area.hovered) {
        const ImVec2 mouse = ImGui::GetMousePos();
        const Keypress* hit = nullptr;
        double bestDistance = 0.0;
        bool haveHit = false;
        for (const auto& entry : drawn) {
            const Keypress& keypress = entry.get();
            const auto index = keyIndices.find(keypress.key);
            if (index == keyIndices.end())
                continue;

            const double rowHeight = keyCount > 0.0 ? area.size.y / keyCount : 0.0;
            const double margin = std::min(kBarMargin, rowHeight / 4.0);
            const double x0 = toPixelX(area, sync.xMin, sync.xMax, keypress.start);
            const double x1 = toPixelX(area, sync.xMin, sync.xMax, keypress.end);
            const double yTop = toPixelY(area, -0.5, keyCount - 0.5, index->second + 0.5);
            const double yBottom = toPixelY(area, -0.5, keyCount - 0.5, index->second - 0.5);

            const double left = x0;
            const double right = std::max(x1, x0 + 1.0);
            const double top = yTop + margin;
            const double bottom = yBottom - margin;

            if (mouse.x < left || mouse.x > right || mouse.y < top || mouse.y > bottom)
                continue;

            const double cx = (left + right) / 2.0;
            const double cy = (top + bottom) / 2.0;
            const double dx = mouse.x - cx;
            const double dy = mouse.y - cy;
            const double distance = dx * dx + dy * dy;
            if (!haveHit || distance < bestDistance) {
                bestDistance = distance;
                haveHit = true;
                hit = &keypress;
            }
        }

        if (hit != nullptr) {
            const double duration = std::max(0.0, hit->end - hit->start);
            showTooltip(mouse,
                { std::format("device: {}", hit->deviceId),
                    std::format("key: {}", hit->key),
                    std::format("start: {:.3f}s", hit->start),
                    std::format("end: {:.3f}s", hit->end),
                    std::format("duration: {:.3f}s", duration) });
        }
    }

    ImGui::PopID();
}

} // namespace Kbi::ui
