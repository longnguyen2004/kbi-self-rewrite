#include "ui/timeline_chart.h"
#include "ui/charts.h"
#include "ui/charts_common.h"

#include "core/input_timeline.h"

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

} // namespace

TimelineChart::TimelineChart(
    const InputTimeline& timeline, const std::vector<std::string>& deviceIds)
    : m_timeline(timeline)
    , m_deviceIds(deviceIds)
{
}

void TimelineChart::draw()
{
    const InputTimeline& timeline = m_timeline;
    const std::vector<std::string>& deviceIds = m_deviceIds;
    const std::string_view id = "##input_timeline";
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    ImGui::PushID(id.data());

    const std::vector<std::string>& keySet = timeline.keys();
    const std::vector<Keypress>& keypresses = timeline.keypresses();
    const double keyCount = static_cast<double>(keySet.size());

    // Map every device to a colour, in the order the devices were seen.
    std::map<std::string, ImU32> deviceColors;
    for (std::size_t i = 0; i < deviceIds.size(); ++i)
        deviceColors[deviceIds[i]]
            = ImGui::ColorConvertFloat4ToU32(kDevicePalette[i % kDevicePalette.size()]);

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

    ChartSync& sync = *m_sync;
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
        std::vector<double> tickValues;
        std::vector<const char*> tickLabels;
        tickValues.reserve(keySet.size());
        tickLabels.reserve(keySet.size());
        for (const std::string& key : keySet) {
            tickValues.push_back(static_cast<double>(tickValues.size()));
            tickLabels.push_back(key.c_str());
        }
        ImPlot::SetupAxisTicks(
            ImAxis_Y1, tickValues.data(), static_cast<int>(tickValues.size()), tickLabels.data());
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
                { std::format("device: {}", hit->deviceId), std::format("key: {}", hit->key),
                    std::format("start: {:.3f}s", hit->start),
                    std::format("end: {:.3f}s", hit->end),
                    std::format("duration: {:.3f}s", duration) });
        }
    }

    ImGui::PopID();
}

void TimelineChart::resetZoom() { *m_sync = ChartSync {}; }

void TimelineChart::setTimelineRange(double xMin, double xMax)
{
    m_sync->initialized = true;
    m_sync->xMin = xMin;
    m_sync->xMax = xMax;
    // The range was set explicitly, it does not follow the end of a recording.
    m_sync->followEnd = false;
}

} // namespace Kbi::ui
