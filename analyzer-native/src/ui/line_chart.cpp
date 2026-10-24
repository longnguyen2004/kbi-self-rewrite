#include "ui/line_chart.h"

#include "core/decimate.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Kbi::Ui {
namespace {

constexpr ImVec4 kDiffColor(65.0f / 255.0f, 140.0f / 255.0f, 240.0f / 255.0f, 1.0f);
constexpr ImVec4 kFreqColor(240.0f / 255.0f, 120.0f / 255.0f, 0.0f, 1.0f);

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
    const std::size_t tickCount = lastIndex < firstIndex
        ? 0
        : static_cast<std::size_t>(std::llround(lastIndex - firstIndex)) + 1;

    std::vector<double> ticks;
    ticks.push_back(min);
    for (std::size_t i = 0; i < tickCount; ++i)
        ticks.push_back(quantizedMin + static_cast<double>(i) * step);
    ticks.push_back(max);
    return ticks;
}

std::vector<double> hertzTicks(double min, double max)
{
    const auto steps = static_cast<std::uint64_t>(std::floor((max - min) / 1000.0));
    const double step
        = std::clamp(125.0 * static_cast<double>(std::bit_ceil(steps)), 125.0, 8000.0);
    const double firstIndex = std::floor(min / step + 1.0);
    const double lastIndex = std::ceil(max / step - 1.0);
    const double quantizedMin = firstIndex * step;
    const std::size_t tickCount = lastIndex < firstIndex
        ? 0
        : static_cast<std::size_t>(std::llround(lastIndex - firstIndex)) + 1;

    std::vector<double> ticks;
    ticks.push_back(min);
    for (std::size_t i = 0; i < tickCount; ++i)
        ticks.push_back(quantizedMin + static_cast<double>(i) * step);
    ticks.push_back(max);
    return ticks;
}

} // namespace

LineChart::LineChart(std::string id, std::string title, ImVec4 color, std::string xLabel,
    std::string yLabel, double initialSpan, TickStyle tickStyle, TooltipCallback tooltip,
    ChartSync* sharedSync, double extentMax)
    : m_id(std::move(id))
    , m_title(std::move(title))
    , m_color(color)
    , m_xLabel(std::move(xLabel))
    , m_yLabel(std::move(yLabel))
    , m_initialSpan(initialSpan)
    , m_tickStyle(tickStyle)
    , m_tooltip(std::move(tooltip))
    , m_extentMax(extentMax)
{
    if (sharedSync != nullptr)
        m_sync = sharedSync;
}

DiffChart::DiffChart(std::string id, std::string title, ChartSync* sharedSync)
    : LineChart(
          std::move(id), std::move(title), kDiffColor, "ms", "events", 25.0,
          TickStyle::Milliseconds,
          [](const ChartPoint& point) {
              return std::vector<std::string> {
                  std::format("x: {:.3f}ms", point.x),
                  std::format("y: {} events", point.y),
              };
          },
          sharedSync, 1000.0)
{
}

FreqChart::FreqChart(std::string id, std::string title, ChartSync* sharedSync)
    : LineChart(
          std::move(id), std::move(title), kFreqColor, "Hz", "magnitude", 0.0, TickStyle::Hertz,
          [](const ChartPoint& point) {
              return std::vector<std::string> {
                  std::format("freq: {}Hz", std::lround(point.x)),
                  std::format("mag: {:.3f}", point.y),
              };
          },
          sharedSync, 0.0)
{
}

void FreqChart::draw(const std::vector<double>& data, double yMax, double maxFrequency)
{
    m_extentMax = maxFrequency;
    m_initialSpan = maxFrequency;
    LineChart::draw(data, yMax);
}

void LineChart::draw(const std::vector<double>& data, double yMax)
{
    ChartSync& sync = this->sync();
    sync.extentMin = 0.0;
    sync.extentMax = m_extentMax;

    const double extentMin = sync.extentMin;
    const double extentMax = sync.extentMax;
    const double extentSpan = std::max(extentMax - extentMin, 1e-12);

    if (!sync.initialized || sync.dataLength != data.size()) {
        sync.initialized = true;
        sync.dataLength = data.size();
        sync.xMin = extentMin;
        sync.xMax
            = extentMin + std::min(m_initialSpan > 0.0 ? m_initialSpan : extentSpan, extentSpan);
    }
    sync.xMin = std::clamp(sync.xMin, extentMin, extentMax);
    sync.xMax = std::clamp(sync.xMax, extentMin, extentMax);
    if (sync.xMax <= sync.xMin)
        sync.xMax = std::min(extentMax, sync.xMin + std::max(extentSpan * 1e-9, 1e-12));

    ImGui::PushID(m_id.c_str());
    if (!m_title.empty())
        ImGui::TextUnformatted(m_title.c_str());

    if (!ImPlot::BeginPlot(
            "##plot", ImVec2(-1.0f, m_chartHeight), ImPlotFlags_NoTitle | ImPlotFlags_NoInputs)) {
        ImPlot::EndPlot();
        ImGui::PopID();
        return;
    }

    const double yRange = yMax > 0.0 ? yMax : 1.0;

    ImPlot::SetupAxis(ImAxis_X1, m_xLabel.c_str());
    ImPlot::SetupAxis(ImAxis_Y1, m_yLabel.c_str());
    ImPlot::SetupAxisLimits(ImAxis_X1, sync.xMin, sync.xMax, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, yRange, ImPlotCond_Always);

    if (m_tickStyle != TickStyle::Automatic) {
        const std::vector<double> generated = m_tickStyle == TickStyle::Milliseconds
            ? millisecondTicks(sync.xMin, sync.xMax)
            : hertzTicks(sync.xMin, sync.xMax);
        std::vector<double> values;
        std::vector<std::string> labelStorage;
        for (const double value : generated) {
            std::string label;
            if (m_tickStyle == TickStyle::Milliseconds) {
                const double fractional = value - std::floor(value);
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
        ImPlot::SetupAxisTicks(
            ImAxis_X1, values.data(), static_cast<int>(values.size()), labels.data());
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
        spec.LineColor = m_color;
        spec.LineWeight = 1.0f;
        spec.Flags = static_cast<ImPlotItemFlags>(ImPlotItemFlags_NoLegend);
        ImPlot::PlotLine("##series", xs.data(), ys.data(), static_cast<int>(xs.size()), spec);
    }

    handleInteraction(sync, area);

    std::vector<SnapPoint> snapPoints;
    snapPoints.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        snapPoints.push_back(SnapPoint { .x = toPixelX(area, sync.xMin, sync.xMax, points[i].x),
            .y = toPixelY(area, 0.0, yRange, points[i].y),
            .index = static_cast<int>(i) });
    }
    m_snapper.set(std::move(snapPoints));

    ImPlot::EndPlot();

    if (area.hovered) {
        const ImVec2 mouse = ImGui::GetMousePos();
        if (const SnapPoint* snapped = m_snapper.find(mouse.x, mouse.y, kTooltipRadius)) {
            const ChartPoint& point = points[static_cast<std::size_t>(snapped->index)];
            const std::vector<std::string> lines = m_tooltip(point);
            showTooltip({ static_cast<float>(snapped->x), static_cast<float>(snapped->y) }, lines);
            ImDrawList* markerList = ImGui::GetWindowDrawList();
            markerList->PushClipRect(ImVec2(area.position.x, area.position.y),
                ImVec2(area.position.x + area.size.x, area.position.y + area.size.y), true);
            const ImU32 markerColor = ImGui::ColorConvertFloat4ToU32(m_color);
            markerList->AddCircleFilled(
                ImVec2(static_cast<float>(snapped->x), static_cast<float>(snapped->y)), 4.0f,
                markerColor);
            markerList->AddCircle(
                ImVec2(static_cast<float>(snapped->x), static_cast<float>(snapped->y)), 4.0f,
                IM_COL32(0, 0, 0, 200), 12, 1.5f);
            markerList->PopClipRect();
        }
    }

    ImGui::PopID();
}

} // namespace Kbi::ui
