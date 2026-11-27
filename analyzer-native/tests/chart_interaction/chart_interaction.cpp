#include <tests/tests.h>

#include <cmath>
#include <print>
#include <ui/charts.h>

namespace Kbi::Tests {
void testChartInteraction()
{
    std::println("chart interaction");

    Kbi::Ui::ChartSync sync;
    sync.extentMin = 0.0;
    sync.extentMax = 1000.0;
    sync.xMin = 100.0;
    sync.xMax = 125.0;

    // Dragging to the right moves the view to the left.
    Kbi::Ui::ChartInteraction drag;
    drag.dragging = true;
    drag.dragDeltaX = 50.0f;
    drag.plotWidth = 500.0f;
    sync.updateRange(drag);
    check(std::abs(sync.xMin - 97.5) < 1e-9, "dragging pans the view");
    check(std::abs(sync.xMax - 122.5) < 1e-9, "dragging keeps the span");

    // The view cannot leave the data extent.
    Kbi::Ui::ChartInteraction bigDrag;
    bigDrag.dragging = true;
    bigDrag.dragDeltaX = -10000.0f;
    bigDrag.plotWidth = 500.0f;
    sync.updateRange(bigDrag);
    check(sync.xMax <= sync.extentMax + 1e-9, "the view is clamped to the extent");
    check(sync.xMax - sync.xMin > 0.0, "the view keeps a positive span");

    // Scrolling zooms around the cursor.
    Kbi::Ui::ChartSync zoomSync;
    zoomSync.extentMin = 0.0;
    zoomSync.extentMax = 1000.0;
    zoomSync.xMin = 0.0;
    zoomSync.xMax = 100.0;
    Kbi::Ui::ChartInteraction zoom;
    zoom.wheel = 1.0f;
    zoom.mouseX = 50.0;
    zoomSync.updateRange(zoom);
    check(zoomSync.xMax - zoomSync.xMin < 100.0, "scrolling zooms in");
    check(std::abs((zoomSync.xMin + zoomSync.xMax) * 0.5 - 50.0) < 1e-6,
        "zooming keeps the cursor position");

    Kbi::Ui::ChartInteraction zoomOut;
    zoomOut.wheel = -1.0f;
    zoomOut.mouseX = 50.0;
    zoomSync.updateRange(zoomOut);
    check(zoomSync.xMax - zoomSync.xMin > 90.0, "scrolling down zooms out");

    // Zooming out never goes beyond the extent.
    for (int i = 0; i < 100; ++i) {
        Kbi::Ui::ChartInteraction step;
        step.wheel = -1.0f;
        step.mouseX = 500.0;
        zoomSync.updateRange(step);
    }
    check(std::abs(zoomSync.xMin - 0.0) < 1e-9 && std::abs(zoomSync.xMax - 1000.0) < 1e-9,
        "zooming out stops at the extent");

    // Zooming in is capped as well: the smallest visible span is the extent
    // divided by the max zoom factor, like the web app's d3 scaleExtent.
    Kbi::Ui::ChartSync cappedSync;
    cappedSync.extentMin = 0.0;
    cappedSync.extentMax = 1000.0;
    cappedSync.xMin = 0.0;
    cappedSync.xMax = 1000.0;
    cappedSync.maxZoom = 1000.0;
    Kbi::Ui::ChartInteraction deepZoom;
    deepZoom.wheel = 1.0f;
    deepZoom.mouseX = 500.0;
    for (int i = 0; i < 100; ++i)
        cappedSync.updateRange(deepZoom);
    check(std::abs((cappedSync.xMax - cappedSync.xMin) - 1.0) < 1e-9,
        "zooming in stops at the extent divided by the max zoom");

    // A double click resets the view.
    Kbi::Ui::ChartInteraction reset;
    reset.doubleClicked = true;
    zoomSync.updateRange(reset);
    check(zoomSync.xMin == 0.0 && zoomSync.xMax == 1000.0, "a double click resets the view");
}

} // namespace Kbi::Tests
