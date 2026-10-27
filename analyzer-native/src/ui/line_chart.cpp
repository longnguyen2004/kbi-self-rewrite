#include <ui/line_chart.h>

#include <core/decimate.h>

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
#include <utility>
#include <vector>

namespace Kbi::Ui {
namespace {

constexpr ImVec4 kDiffColor(65.0f / 255.0f, 140.0f / 255.0f, 240.0f / 255.0f, 1.0f);
constexpr ImVec4 kFreqColor(240.0f / 255.0f, 120.0f / 255.0f, 0.0f, 1.0f);

// The tick step of the millisecond style: a readable ladder of fractions and
// multiples of a millisecond.
double millisecondTickStep(double range)
{
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
    return step;
}

// The tick step of the hertz style: multiples of 125Hz, the base bin rate.
double hertzTickStep(double range)
{
    const auto steps = static_cast<std::uint64_t>(std::floor(range / 1000.0));
    return std::clamp(125.0 * static_cast<double>(std::bit_ceil(steps)), 125.0, 8000.0);
}

// The regular ticks between min and max with the given step, plus the exact
// range ends (F.1 — the tail of both tick styles was identical).
void appendRegularTicks(double min, double max, double step, std::vector<double>& out)
{
    const double firstIndex = std::floor(min / step + 1.0);
    const double lastIndex = std::ceil(max / step - 1.0);
    const double quantizedMin = firstIndex * step;
    const std::size_t tickCount = lastIndex < firstIndex
        ? 0
        : static_cast<std::size_t>(std::llround(lastIndex - firstIndex)) + 1;

    out.clear();
    out.push_back(min);
    for (std::size_t i = 0; i < tickCount; ++i)
        out.push_back(quantizedMin + static_cast<double>(i) * step);
    out.push_back(max);
}

} // namespace

LineChart::LineChart(Config config)
    : m_id(std::move(config.id))
    , m_title(std::move(config.title))
    , m_color(config.color)
    , m_xLabel(std::move(config.xLabel))
    , m_yLabel(std::move(config.yLabel))
    , m_initialSpan(config.initialSpan)
    , m_tickStyle(config.tickStyle)
    , m_tooltip(std::move(config.tooltip))
    , m_extentMax(config.extentMax)
{
    useSharedSync(config.sharedSync);
}

DiffChart::DiffChart(std::string id, std::string title, ChartSync* sharedSync)
    : LineChart({ .id = std::move(id),
          .title = std::move(title),
          .color = kDiffColor,
          .xLabel = "ms",
          .yLabel = "events",
          .initialSpan = 25.0,
          .tickStyle = TickStyle::Milliseconds,
          .tooltip =
              [](ChartPoint point) {
                  return std::vector<std::string> {
                      std::format("x: {:.3f}ms", point.x),
                      std::format("y: {} events", point.y),
                  };
              },
          .sharedSync = sharedSync,
          .extentMax = 1000.0 })
{
}

FreqChart::FreqChart(std::string id, std::string title, ChartSync* sharedSync)
    : LineChart({ .id = std::move(id),
          .title = std::move(title),
          .color = kFreqColor,
          .xLabel = "Hz",
          .yLabel = "magnitude",
          .initialSpan = 0.0,
          .tickStyle = TickStyle::Hertz,
          .tooltip =
              [](ChartPoint point) {
                  return std::vector<std::string> {
                      std::format("freq: {}Hz", std::lround(point.x)),
                      std::format("mag: {:.3f}", point.y),
                  };
              },
          .sharedSync = sharedSync,
          .extentMax = 0.0 })
{
}

void FreqChart::draw(const std::vector<double>& data, double yMax, double maxFrequency)
{
    setExtentMax(maxFrequency);
    setInitialSpan(maxFrequency);
    LineChart::draw(data, yMax);
}

void LineChart::updateExtent(ChartSync& sync, std::size_t dataSize) const
{
    sync.extentMin = 0.0;
    sync.extentMax = m_extentMax;

    const double extentMin = sync.extentMin;
    const double extentMax = sync.extentMax;
    const double extentSpan = std::max(extentMax - extentMin, 1e-12);

    if (!sync.initialized || sync.dataLength != dataSize) {
        sync.initialized = true;
        sync.dataLength = dataSize;
        sync.xMin = extentMin;
        sync.xMax
            = extentMin + std::min(m_initialSpan > 0.0 ? m_initialSpan : extentSpan, extentSpan);
    }
    sync.xMin = std::clamp(sync.xMin, extentMin, extentMax);
    sync.xMax = std::clamp(sync.xMax, extentMin, extentMax);
    if (sync.xMax <= sync.xMin)
        sync.xMax = std::min(extentMax, sync.xMin + std::max(extentSpan * 1e-9, 1e-12));
}

void LineChart::setupAxes(ChartSync& sync, double yRange)
{
    ImPlot::SetupAxis(ImAxis_X1, m_xLabel.c_str());
    ImPlot::SetupAxis(ImAxis_Y1, m_yLabel.c_str());
    ImPlot::SetupAxisLimits(ImAxis_X1, sync.xMin, sync.xMax, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, yRange, ImPlotCond_Always);

    if (m_tickStyle == TickStyle::Automatic)
        return;

    const double range = sync.xMax - sync.xMin;
    if (m_tickStyle == TickStyle::Milliseconds)
        appendRegularTicks(sync.xMin, sync.xMax, millisecondTickStep(range), m_tickGenerated);
    else
        appendRegularTicks(sync.xMin, sync.xMax, hertzTickStep(range), m_tickGenerated);

    // Label the ticks; the values that do not map to a clean label are
    // dropped.
    m_tickValues.clear();
    m_tickLabels.clear();
    for (const double value : m_tickGenerated) {
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
        m_tickValues.push_back(value);
        m_tickLabels.push_back(std::move(label));
    }
    m_tickLabelPtrs.clear();
    m_tickLabelPtrs.reserve(m_tickLabels.size());
    for (const std::string& label : m_tickLabels)
        m_tickLabelPtrs.push_back(label.c_str());
    ImPlot::SetupAxisTicks(ImAxis_X1, m_tickValues.data(), static_cast<int>(m_tickValues.size()),
        m_tickLabelPtrs.data());
}

void LineChart::drawSeries(const std::vector<double>& data, const PlotArea& area, ChartSync& sync,
    double extentMin, double extentMax)
{
    const int width = static_cast<int>(area.size.x);
    decimate(data, { .min = extentMin, .max = extentMax }, { .min = sync.xMin, .max = sync.xMax },
        width, m_points);
    if (m_points.empty())
        return;

    m_xs.clear();
    m_ys.clear();
    m_xs.reserve(m_points.size());
    m_ys.reserve(m_points.size());
    for (const ChartPoint& point : m_points) {
        m_xs.push_back(point.x);
        m_ys.push_back(point.y);
    }
    ImPlotSpec spec;
    spec.LineColor = m_color;
    spec.LineWeight = 1.0f;
    spec.Flags = static_cast<ImPlotItemFlags>(ImPlotItemFlags_NoLegend);
    ImPlot::PlotLine("##series", m_xs.data(), m_ys.data(), static_cast<int>(m_xs.size()), spec);
}

void LineChart::updateSnapPoints(const PlotArea& area, ChartSync& sync, double yRange)
{
    m_snapPoints.clear();
    m_snapPoints.reserve(m_points.size());
    for (std::ptrdiff_t i = 0; std::cmp_less(i, m_points.size()); ++i) {
        const ChartPoint& point = m_points[static_cast<std::size_t>(i)];
        m_snapPoints.push_back(
            SnapPoint { .x = toPixelX(area, { .min = sync.xMin, .max = sync.xMax }, point.x),
                .y = toPixelY(area, { .min = 0.0, .max = yRange }, point.y),
                .index = static_cast<int>(i) });
    }
    m_snapper.set(m_snapPoints);
}

void LineChart::drawTooltipMarker(const PlotArea& area, double yRange)
{
    if (!area.hovered)
        return;
    const ImVec2 mouse = ImGui::GetMousePos();
    const SnapPoint* snapped = m_snapper.find(mouse.x, mouse.y, kTooltipRadius);
    if (snapped == nullptr)
        return;
    const ChartPoint& point = m_points[static_cast<std::size_t>(snapped->index)];
    const std::vector<std::string> lines = m_tooltip(point);
    showTooltip({ static_cast<float>(snapped->x), static_cast<float>(snapped->y) }, lines);
    ImDrawList* markerList = ImGui::GetWindowDrawList();
    markerList->PushClipRect(ImVec2(area.position.x, area.position.y),
        ImVec2(area.position.x + area.size.x, area.position.y + area.size.y), true);
    const ImU32 markerColor = ImGui::ColorConvertFloat4ToU32(m_color);
    markerList->AddCircleFilled(
        ImVec2(static_cast<float>(snapped->x), static_cast<float>(snapped->y)), 4.0f, markerColor);
    markerList->AddCircle(ImVec2(static_cast<float>(snapped->x), static_cast<float>(snapped->y)),
        4.0f, IM_COL32(0, 0, 0, 200), 12, 1.5f);
    markerList->PopClipRect();
}

void LineChart::draw(const std::vector<double>& data, double yMax)
{
    ChartSync& sync = this->sync();
    updateExtent(sync, data.size());

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
    setupAxes(sync, yRange);

    const PlotArea area = currentPlotArea({ .min = sync.xMin, .max = sync.xMax });
    drawSeries(data, area, sync, sync.extentMin, sync.extentMax);

    handleInteraction(sync, area);
    updateSnapPoints(area, sync, yRange);

    ImPlot::EndPlot();

    drawTooltipMarker(area, yRange);

    ImGui::PopID();
}

} // namespace Kbi::Ui
