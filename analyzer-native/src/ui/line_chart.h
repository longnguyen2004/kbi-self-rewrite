#pragma once

#include <core/decimate.h>
#include <ui/charts.h>
#include <ui/charts_common.h>

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
    using TooltipCallback = std::function<std::vector<std::string>(ChartPoint)>;

    // The fixed configuration, supplied at construction (I.23/F.2 — one
    // bundle instead of ten loosely typed parameters).
    struct Config {
        std::string id;
        std::string title;
        ImVec4 color { 1.0f, 1.0f, 1.0f, 1.0f };
        std::string xLabel;
        std::string yLabel;
        double initialSpan = 0.0;
        TickStyle tickStyle = TickStyle::Automatic;
        TooltipCallback tooltip;
        ChartSync* sharedSync = nullptr;
        double extentMax = 0.0;
    };

    explicit LineChart(Config config);

    void setChartHeight(float height) { m_chartHeight = height; }

    // The parameterless draw of the base stays part of the overload set
    // (C.138).
    using Chart::draw;

    void draw(const std::vector<double>& data, double yMax);

private:
    // Recomputes the visible range of `sync` for the new data length.
    void updateExtent(ChartSync& sync, std::size_t dataSize) const;
    // Sets up the axes and the generated ticks.
    void setupAxes(ChartSync& sync, double yRange);
    // Decimates `data` into the scratch buffer and draws the line.
    void drawSeries(const std::vector<double>& data, const PlotArea& area, ChartSync& sync,
        double extentMin, double extentMax);
    // Rebuilds the snap points of the drawn series.
    void updateSnapPoints(const PlotArea& area, ChartSync& sync, double yRange);
    // Draws the tooltip and the marker of the snapped point, if any.
    void drawTooltipMarker(const PlotArea& area, double yRange);

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

    // Scratch buffers reused across frames (Per.15 — the chart draws every
    // frame, nothing here may allocate per frame).
    std::vector<ChartPoint> m_points;
    std::vector<double> m_xs;
    std::vector<double> m_ys;
    std::vector<SnapPoint> m_snapPoints;
    std::vector<double> m_tickGenerated;
    std::vector<double> m_tickValues;
    std::vector<std::string> m_tickLabels;
    std::vector<const char*> m_tickLabelPtrs;

protected:
    // FreqChart adjusts the extent right before it draws (C.133 — the data
    // members themselves are private).
    void setExtentMax(double extentMax) { m_extentMax = extentMax; }
    void setInitialSpan(double initialSpan) { m_initialSpan = initialSpan; }
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

    // Keep the base overload sets visible (C.138).
    using LineChart::draw;

    void draw(const std::vector<double>& data, double yMax, double maxFrequency);
};

} // namespace Kbi::Ui
