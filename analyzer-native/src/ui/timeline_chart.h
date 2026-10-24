#pragma once

#include "core/input_timeline.h"
#include "ui/chart.h"
#include "ui/charts_common.h"

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
    void setTimelineRange(double xMin, double xMax);

private:
    const InputTimeline& m_timeline;
    const std::vector<std::string>& m_deviceIds;
};

} // namespace Kbi::Ui
