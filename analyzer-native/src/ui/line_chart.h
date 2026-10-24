#pragma once

#include "ui/charts.h"
#include "ui/charts_common.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Kbi::Ui {

// Draws the diff and frequency line charts with ImPlot.
class LineChartRenderer {
public:
    // Time domain histogram of the timestamp differences.
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

    void setChartHeight(float height) { m_chartHeight = height; }

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
    PointSnapper m_snapper;
};

} // namespace Kbi::Ui
