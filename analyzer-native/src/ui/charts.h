#pragma once

#include "ui/charts_common.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace Kbi::Ui {

// The user interaction of a chart, in a form that does not depend on ImGui.
struct ChartInteraction {
    bool dragging = false;
    float dragDeltaX = 0.0f; // pixels
    float wheel = 0.0f; // scroll steps
    double mouseX = 0.0; // mouse position in data units
    bool doubleClicked = false;
    float plotWidth = 0.0f; // pixels
};

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

    // Applies an interaction to the visible range. The range is clamped to
    // the extent of the data.
    void updateRange(const ChartInteraction& interaction);
};

// The common base of every chart. It owns the zoom state, so that it does not
// have to be passed around.
class Chart {
public:
    Chart() = default;
    virtual ~Chart() = default;

    virtual void draw() { }

    ChartSync& sync() { return *m_sync; }

    virtual void resetZoom() { *m_sync = ChartSync {}; }

    Chart(const Chart&) = delete;
    Chart& operator=(const Chart&) = delete;
    Chart(Chart&&) = delete;
    Chart& operator=(Chart&&) = delete;

protected:
    ChartSync m_ownedSync;
    ChartSync* m_sync = &m_ownedSync;
};

} // namespace Kbi::Ui
