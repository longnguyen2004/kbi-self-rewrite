#pragma once

#include <algorithm>
#include <vector>

namespace Kbi::Ui {

// A point of a chart, in pixel coordinates.
struct SnapPoint {
    double x = 0.0;
    double y = 0.0;
    int index = 0; // the index of the point in the source data
};

// The nearest neighbour hit test used by the chart tooltips.
//
// The web analyzer uses a d3 quadtree over the pixel positions of the data.
// The points of a chart are sorted by their x coordinate here, so the search
// reduces to a binary search followed by a scan of the points that are close
// enough, which gives the same result.
class PointSnapper {
public:
    // The points are copied so that the internal storage is reused across
    // frames (Per.15).
    void set(const std::vector<SnapPoint>& points)
    {
        m_points = points;
        std::sort(m_points.begin(), m_points.end(),
            [](const SnapPoint& a, const SnapPoint& b) { return a.x < b.x; });
    }

    // Returns the point that is closest to (x, y), but only when it is within
    // the given radius. Returns nullptr when there is none.
    [[nodiscard]] const SnapPoint* find(double x, double y, double radius) const
    {
        if (m_points.empty())
            return nullptr;

        const auto begin = std::lower_bound(m_points.begin(), m_points.end(), x - radius,
            [](const SnapPoint& point, double value) { return point.x < value; });
        const auto end = std::upper_bound(m_points.begin(), m_points.end(), x + radius,
            [](double value, const SnapPoint& point) { return value < point.x; });

        const SnapPoint* best = nullptr;
        double bestDistance = radius * radius;
        for (auto it = begin; it != end; ++it) {
            const double dx = it->x - x;
            const double dy = it->y - y;
            const double distance = dx * dx + dy * dy;
            if (distance <= bestDistance) {
                bestDistance = distance;
                best = &*it;
            }
        }
        return best;
    }

private:
    std::vector<SnapPoint> m_points;
};

} // namespace Kbi::Ui
