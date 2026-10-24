#pragma once

#include "core/decimate.h"
#include "ui/charts.h"
#include "ui/charts_common.h"

#include <functional>
#include <imgui.h>
#include <implot.h>
#include <string>
#include <string_view>
#include <vector>

namespace Kbi::Ui {

// How the ticks of the x axis are generated.
enum class TickStyle {
    // Steps of 0.125, 0.25, 0.5, 1, 2, 5, ... milliseconds.
    Milliseconds,
    // Steps of 125Hz, 250Hz, 500Hz, ... hertz.
    Hertz,
    // The default ticks of ImPlot.
    Automatic,
};

// A line chart: a series of points connected by a line, with a tooltip on the
// nearest point. Its configuration is fixed at construction time, the data is
// supplied when it is drawn, so that the configuration does not have to be
// passed around every frame.
class LineChart : public Chart {
public:
    using TooltipCallback = std::function<std::vector<std::string>(const ChartPoint&)>;

    LineChart(std::string id, std::string title, ImVec4 color, std::string xLabel,
        std::string yLabel, double initialSpan, TickStyle tickStyle, TooltipCallback tooltip,
        ChartSync* sharedSync = nullptr, double extentMax = 0.0);

    void setChartHeight(float height) { m_chartHeight = height; }

    void draw(const std::vector<double>& data, double yMax);

protected:
    void drawLineChart(const std::vector<double>& data, double yMax);

    std::string m_id;
    std::string m_title;
    ImVec4 m_color;
    std::string m_xLabel;
    std::string m_yLabel;
    double m_initialSpan;
    TickStyle m_tickStyle;
    TooltipCallback m_tooltip;
    float m_chartHeight = 220.0f;
    double m_extentMax = 0.0;
    PointSnapper m_snapper;
};

// A diff chart: a line chart over a one second window in milliseconds.
class DiffChart : public LineChart {
public:
    DiffChart(std::string id, std::string title, ChartSync* sharedSync = nullptr);
};

// A frequency chart: a line chart over a frequency range in hertz.
class FreqChart : public LineChart {
public:
    FreqChart(std::string id, std::string title, ChartSync* sharedSync = nullptr);

    void draw(const std::vector<double>& data, double yMax, double maxFrequency);
};

} // namespace Kbi::Ui
