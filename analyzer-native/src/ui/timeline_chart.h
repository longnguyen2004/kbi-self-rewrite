#pragma once

#include "core/input_timeline.h"
#include "ui/charts.h"
#include "ui/charts_common.h"

#include <string>
#include <string_view>
#include <vector>

namespace Kbi::Ui {

// Draws the Gantt style chart of the keypress intervals.
class TimelineChartRenderer {
public:
    void drawInputTimeline(std::string_view id,
        const InputTimeline& timeline,
        const std::vector<std::string>& deviceIds);

    // Resets the zoom state.
    void resetZoom();

    // Sets the visible range of the input timeline.
    void setTimelineRange(double xMin, double xMax);

private:
    ChartSync m_timelineSync;
};

} // namespace Kbi::Ui
