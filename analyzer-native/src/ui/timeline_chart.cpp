#include <ui/charts.h>
#include <ui/charts_common.h>
#include <ui/timeline_chart.h>

#include <core/input_timeline.h>

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <format>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Kbi::Ui {
namespace {

constexpr std::array<ImVec4, 6> kDevicePalette = {
    ImVec4(65.0f / 255.0f, 140.0f / 255.0f, 240.0f / 255.0f, 1.0f),
    ImVec4(240.0f / 255.0f, 120.0f / 255.0f, 0.0f, 1.0f),
    ImVec4(80.0f / 255.0f, 200.0f / 255.0f, 120.0f / 255.0f, 1.0f),
    ImVec4(220.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f, 1.0f),
    ImVec4(180.0f / 255.0f, 100.0f / 255.0f, 220.0f / 255.0f, 1.0f),
    ImVec4(240.0f / 255.0f, 200.0f / 255.0f, 60.0f / 255.0f, 1.0f),
};

// The pixel rectangle of a keypress bar, with the row margins applied. Both
// the bar drawing and the tooltip hit test use it (F.1 — the geometry was
// duplicated).
struct BarRect {
    double left;
    double right;
    double top;
    double bottom;
};

BarRect barRect(const PlotArea& area, const ChartSync& sync, double keyCount, double keyIndex,
    double start, double end)
{
    const double rowHeight = keyCount > 0.0 ? area.size.y / keyCount : 0.0;
    const double margin = std::min(kBarMargin, rowHeight / 4.0);
    const double x0 = toPixelX(area, { .min = sync.xMin, .max = sync.xMax }, start);
    const double x1 = toPixelX(area, { .min = sync.xMin, .max = sync.xMax }, end);
    const double yTop = toPixelY(area, { .min = -0.5, .max = keyCount - 0.5 }, keyIndex + 0.5);
    const double yBottom = toPixelY(area, { .min = -0.5, .max = keyCount - 0.5 }, keyIndex - 0.5);
    return { .left = x0,
        .right = std::max(x1, x0 + 1.0),
        .top = yTop + margin,
        .bottom = std::max(yBottom - margin, yTop + margin + 1.0) };
}

} // namespace

TimelineChart::TimelineChart(
    const InputTimeline& timeline, const std::vector<std::string>& deviceIds)
    : m_timeline(timeline)
    , m_deviceIds(deviceIds)
{
}

void TimelineChart::updateDeviceColors(const std::vector<std::string>& deviceIds)
{
    // The devices are discovered while recording, the list only grows; the
    // colors are assigned in the order the devices were seen.
    if (m_deviceColors.size() == deviceIds.size())
        return;
    m_deviceColors.clear();
    for (std::ptrdiff_t i = 0; std::cmp_less(i, deviceIds.size()); ++i)
        m_deviceColors[deviceIds[static_cast<std::size_t>(i)]] = ImGui::ColorConvertFloat4ToU32(
            kDevicePalette[static_cast<std::size_t>(i) % kDevicePalette.size()]);
}

void TimelineChart::updateKeyIndices(const std::vector<std::string>& keySet)
{
    // The index of every key on the y axis; the key set only grows.
    if (m_keyIndices.size() == keySet.size())
        return;
    m_keyIndices.clear();
    double index = 0.0;
    for (const std::string& key : keySet)
        m_keyIndices.emplace(key, index++);
}

void TimelineChart::updateDrawn(const std::vector<Keypress>& keypresses)
{
    // The keypresses that are drawn, they are the ones the tooltip snaps to.
    // The list only grows while recording; a reset shrinks it, which forces a
    // rebuild.
    if (m_drawn.size() == keypresses.size())
        return;
    m_drawn.clear();
    m_drawn.reserve(keypresses.size());
    for (const Keypress& keypress : keypresses) {
        if (!m_keyIndices.contains(keypress.key))
            continue;
        m_drawn.emplace_back(keypress);
    }
}

void TimelineChart::updateSync(const InputTimeline& timeline)
{
    ChartSync& sync = this->sync();
    const double previousExtentMax = sync.extentMax;
    sync.extentMin = 0.0;
    sync.extentMax = std::max(timeline.endTimestamp(), 1.0);
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
}

void TimelineChart::drawBars(const PlotArea& area, const ChartSync& sync, double keyCount)
{
    if (m_drawn.empty())
        return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 plotMin = area.position;
    const ImVec2 plotMax(area.position.x + area.size.x, area.position.y + area.size.y);
    drawList->PushClipRect(plotMin, plotMax, true);

    for (const Keypress& keypress : m_drawn) {
        // Cull the bars that are outside of the view.
        const double x0 = toPixelX(area, { .min = sync.xMin, .max = sync.xMax }, keypress.start);
        const double x1 = toPixelX(area, { .min = sync.xMin, .max = sync.xMax }, keypress.end);
        if (x1 < plotMin.x || x0 > plotMax.x)
            continue;

        const auto index = m_keyIndices.find(keypress.key);
        if (index == m_keyIndices.end())
            continue;

        const BarRect rect
            = barRect(area, sync, keyCount, index->second, keypress.start, keypress.end);
        const ImVec2 pMin(static_cast<float>(rect.left), static_cast<float>(rect.top));
        const ImVec2 pMax(static_cast<float>(rect.right), static_cast<float>(rect.bottom));

        const ImU32 color = m_deviceColors.contains(keypress.deviceId)
            ? m_deviceColors[keypress.deviceId]
            : IM_COL32(200, 200, 200, 255);
        drawList->AddRectFilled(pMin, pMax, color);
    }

    drawList->PopClipRect();
}

void TimelineChart::drawTooltip(const PlotArea& area, const ChartSync& sync, double keyCount)
{
    if (!area.hovered)
        return;

    // The tooltip of the keypress under the cursor. A bar is hit when the
    // cursor lies inside its rectangle, which is more precise than snapping.
    const ImVec2 mouse = ImGui::GetMousePos();
    const Keypress* hit = nullptr;
    double bestDistance = 0.0;
    bool haveHit = false;
    for (const auto& entry : m_drawn) {
        const Keypress& keypress = entry.get();
        const auto index = m_keyIndices.find(keypress.key);
        if (index == m_keyIndices.end())
            continue;

        const BarRect rect
            = barRect(area, sync, keyCount, index->second, keypress.start, keypress.end);

        if (mouse.x < rect.left || mouse.x > rect.right || mouse.y < rect.top
            || mouse.y > rect.bottom)
            continue;

        const double cx = (rect.left + rect.right) / 2.0;
        const double cy = (rect.top + rect.bottom) / 2.0;
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
            { std::format("device: {}", hit->deviceId), std::format("key: {}", hit->key),
                std::format("start: {:.3f}s", hit->start), std::format("end: {:.3f}s", hit->end),
                std::format("duration: {:.3f}s", duration) });
    }
}

void TimelineChart::draw()
{
    const InputTimeline& timeline = m_timeline;
    const std::vector<std::string>& keySet = timeline.keys();
    const std::vector<Keypress>& keypresses = timeline.keypresses();
    const double keyCount = static_cast<double>(keySet.size());

    updateDeviceColors(m_deviceIds);
    updateKeyIndices(keySet);
    updateDrawn(keypresses);
    updateSync(timeline);
    ChartSync& sync = this->sync();

    ImGui::PushID("##input_timeline");

    // The timeline takes up all the space that is left.
    if (!ImPlot::BeginPlot(
            "##plot", ImVec2(-1.0f, -1.0f), ImPlotFlags_NoTitle | ImPlotFlags_NoInputs)) {
        ImPlot::EndPlot();
        ImGui::PopID();
        return;
    }

    ImPlot::SetupAxis(ImAxis_X1, "s");
    ImPlot::SetupAxisLimits(ImAxis_X1, sync.xMin, sync.xMax, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, -0.5,
        std::max(static_cast<double>(keySet.size()) - 0.5, 0.5), ImPlotCond_Always);

    if (!keySet.empty()) {
        m_tickValues.clear();
        m_tickValues.reserve(keySet.size());
        m_tickLabelPtrs.clear();
        m_tickLabelPtrs.reserve(keySet.size());
        for (const std::string& key : keySet) {
            m_tickValues.push_back(static_cast<double>(m_tickValues.size()));
            // The strings live in the timeline, which outlives the frame.
            m_tickLabelPtrs.push_back(key.c_str());
        }
        ImPlot::SetupAxisTicks(ImAxis_Y1, m_tickValues.data(),
            static_cast<int>(m_tickValues.size()), m_tickLabelPtrs.data());
    }

    const PlotArea area = currentPlotArea({ .min = sync.xMin, .max = sync.xMax });

    handleInteraction(sync, area);

    ImPlot::EndPlot();

    drawBars(area, sync, keyCount);
    drawTooltip(area, sync, keyCount);

    ImGui::PopID();
}

void TimelineChart::resetZoom() { sync() = ChartSync {}; }

void TimelineChart::setTimelineRange(AxisRange range)
{
    ChartSync& sync = this->sync();
    sync.initialized = true;
    sync.xMin = range.min;
    sync.xMax = range.max;
    // The range was set explicitly, it does not follow the end of a recording.
    sync.followEnd = false;
}

} // namespace Kbi::Ui
