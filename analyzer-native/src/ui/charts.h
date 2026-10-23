#pragma once

#include "core/input_timeline.h"
#include "ui/point_snapper.h"

#include <imgui.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Kbi::Ui {

// Zoom state shared by a group of charts that are synchronized along the x
// axis (the three diff charts, and the three frequency charts).
struct ChartSync {
    double xMin = 0.0;
    double xMax = 1.0;
    double extentMin = 0.0;
    double extentMax = 0.0;
    std::size_t dataLength = 0;
    bool initialized = false;
    // Whether the view follows the end of a growing recording.
    bool followEnd = false;
};

// The user interaction of a chart, in a form that does not depend on ImGui.
struct ChartInteraction {
    bool dragging = false;
    float dragDeltaX = 0.0f; // pixels
    float wheel = 0.0f; // scroll steps
    double mouseX = 0.0; // mouse position in data units
    bool doubleClicked = false;
    float plotWidth = 0.0f; // pixels
};

// Applies an interaction to the visible range of a chart. The range is
// clamped to the extent of the data.
void updateRange(ChartSync& sync, const ChartInteraction& interaction);

// What the tooltip of a chart shows.
enum class TooltipStyle {
    // The x axis is a duration, the y axis a number of events.
    Difference,
    // The x axis is a frequency, the y axis a magnitude.
    Frequency,
};

// How the ticks of the x axis are generated.
enum class TickStyle {
    // Steps of 0.125, 0.25, 0.5, 1, 2, 5, ... milliseconds.
    Milliseconds,
    // Steps of 125Hz, 250Hz, 500Hz, ... hertz.
    Hertz,
    // The default ticks of ImPlot.
    Automatic,
};

// Draws the analysis charts with ImPlot.
class ChartRenderer {
public:
    // Time domain histogram of the timestamp differences.
    // The x axis is in milliseconds, the y axis is the number of events.
    void drawDiffChart(std::string_view id,
        std::string_view title,
        const std::vector<double>& data,
        double yMax,
        ChartSync& sync);

    // Frequency domain magnitude of the histograms. The x axis is in Hz.
    void drawFreqChart(std::string_view id,
        std::string_view title,
        const std::vector<double>& data,
        double yMax,
        double maxFrequency,
        ChartSync& sync);

    // Gantt style chart of the keypress intervals.
    void drawInputTimeline(std::string_view id,
        const InputTimeline& timeline,
        const std::vector<std::string>& deviceIds);

    void setChartHeight(float height) { m_chartHeight = height; }

    // Resets the zoom state of every chart.
    void resetZoom();

    // Sets the visible range of the input timeline.
    void setTimelineRange(double xMin, double xMax);

private:
    void drawLineChart(std::string_view id,
        std::string_view title,
        const std::vector<double>& data,
        double yMax,
        ChartSync& sync,
        const ImVec4& color,
        std::string_view xLabel,
        std::string_view yLabel,
        double initialSpan,
        TickStyle tickStyle,
        TooltipStyle tooltipStyle);

    float m_chartHeight = 220.0f;
    ChartSync m_timelineSync;
    PointSnapper m_snapper;
};

} // namespace Kbi::Ui
