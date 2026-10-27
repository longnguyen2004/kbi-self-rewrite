#pragma once

#include <core/input_timeline.h>
#include <ui/charts.h>
#include <ui/charts_common.h>

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Kbi::Ui {

// Draws the Gantt style chart of the keypress intervals.
class TimelineChart : public Chart {
public:
    TimelineChart(const InputTimeline& timeline, const std::vector<std::string>& deviceIds);

    void draw() override;

    // Resets the zoom state.
    void resetZoom() override;

    // Sets the visible range of the input timeline.
    void setTimelineRange(AxisRange range);

private:
    // Rebuilds the caches when the underlying data has grown or shrunk; all
    // of it is reused across frames otherwise (Per.15).
    void updateDeviceColors(const std::vector<std::string>& deviceIds);
    void updateKeyIndices(const std::vector<std::string>& keySet);
    void updateDrawn(const std::vector<Keypress>& keypresses);
    // Extends the visible range to the end of a growing recording.
    void updateSync(const InputTimeline& timeline);
    // Draws the keypress bars on the draw list (ImPlot's line items cannot be
    // made that thick).
    void drawBars(const PlotArea& area, const ChartSync& sync, double keyCount);
    // Shows the tooltip of the keypress bar under the cursor.
    void drawTooltip(const PlotArea& area, const ChartSync& sync, double keyCount);

    const InputTimeline& m_timeline;
    const std::vector<std::string>& m_deviceIds;

    // Scratch caches and buffers reused across frames (Per.15). The keypress
    // list only grows while recording, so size changes are the signal to
    // rebuild.
    std::map<std::string, ImU32> m_deviceColors;
    std::map<std::string, double> m_keyIndices;
    std::vector<std::reference_wrapper<const Keypress>> m_drawn;
    std::vector<double> m_tickValues;
    std::vector<const char*> m_tickLabelPtrs;
};

} // namespace Kbi::Ui
