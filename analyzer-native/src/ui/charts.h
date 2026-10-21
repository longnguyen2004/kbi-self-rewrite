#pragma once

#include "core/input_timeline.h"
#include "ui/point_snapper.h"

#include <imgui.h>

#include <cstddef>
#include <string>
#include <vector>

namespace kbi::ui {

// Zoom state shared by a group of charts that are synchronized along the x
// axis (the three diff charts, and the three frequency charts).
struct ChartSync {
    double x_min = 0.0;
    double x_max = 1.0;
    double extent_min = 0.0;
    double extent_max = 0.0;
    std::size_t data_length = 0;
    bool initialized = false;
    // Whether the view follows the end of a growing recording.
    bool follow_end = false;
};

// The user interaction of a chart, in a form that does not depend on ImGui.
struct ChartInteraction {
    bool dragging = false;
    float drag_delta_x = 0.0f; // pixels
    float wheel = 0.0f;        // scroll steps
    double mouse_x = 0.0;      // mouse position in data units
    bool double_clicked = false;
    float plot_width = 0.0f;   // pixels
};

// Applies an interaction to the visible range of a chart. The range is
// clamped to the extent of the data.
void update_range(ChartSync& sync, const ChartInteraction& interaction);

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
    void draw_diff_chart(const char* id,
                         const char* title,
                         const std::vector<double>& data,
                         double y_max,
                         ChartSync& sync);

    // Frequency domain magnitude of the histograms. The x axis is in Hz.
    void draw_freq_chart(const char* id,
                         const char* title,
                         const std::vector<double>& data,
                         double y_max,
                         double max_frequency,
                         ChartSync& sync);

    // Gantt style chart of the keypress intervals.
    void draw_input_timeline(const char* id,
                             const InputTimeline& timeline,
                             const std::vector<std::string>& device_ids);

    void set_chart_height(float height) { m_chart_height = height; }

    // Resets the zoom state of every chart.
    void reset_zoom();

    // Sets the visible range of the input timeline.
    void set_timeline_range(double x_min, double x_max);

private:
    void draw_line_chart(const char* id,
                         const char* title,
                         const std::vector<double>& data,
                         double y_max,
                         ChartSync& sync,
                         const ImVec4& color,
                         const char* x_label,
                         const char* y_label,
                         double initial_span,
                         TickStyle tick_style,
                         TooltipStyle tooltip_style);

    float m_chart_height = 220.0f;
    ChartSync m_timeline_sync;
    PointSnapper m_snapper;
};

} // namespace kbi::ui
