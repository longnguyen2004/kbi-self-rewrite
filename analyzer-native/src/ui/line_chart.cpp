#include "ui/line_chart.h"
#include "ui/charts.h"
#include "ui/charts_common.h"

#include "core/decimate.h"
#include "ui/point_snapper.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Kbi::Ui {
namespace {

constexpr ImVec4 kDiffColor(65.0f / 255.0f, 140.0f / 255.0f, 240.0f / 255.0f, 1.0f);
constexpr ImVec4 kFreqColor(240.0f / 255.0f, 120.0f / 255.0f, 0.0f, 1.0f);

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

} // namespace

void LineChartRenderer::drawLineChart(std::string_view id,
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
            showTooltip({ static_cast<float>(snapped->x), static_cast<float>(snapped->y) }, lines);
            // A small marker shows where the snap point that triggered the
            // tooltip is, so it is clear which data point is reported.
            ImDrawList* markerList = ImGui::GetWindowDrawList();
            markerList->PushClipRect(ImVec2(area.position.x, area.position.y),
                ImVec2(area.position.x + area.size.x, area.position.y + area.size.y), true);
            const ImU32 markerColor = ImGui::ColorConvertFloat4ToU32(color);
            markerList->AddCircleFilled(ImVec2(static_cast<float>(snapped->x), static_cast<float>(snapped->y)),
                4.0f, markerColor);
            markerList->AddCircle(ImVec2(static_cast<float>(snapped->x), static_cast<float>(snapped->y)),
                4.0f, IM_COL32(0, 0, 0, 200), 12, 1.5f);
            markerList->PopClipRect();
        }
    }

    ImGui::PopID();
}

void LineChartRenderer::drawDiffChart(std::string_view id,
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

void LineChartRenderer::drawFreqChart(std::string_view id,
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

} // namespace Kbi::ui
