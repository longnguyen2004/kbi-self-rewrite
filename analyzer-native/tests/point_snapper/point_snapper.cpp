#include <boost/ut.hpp>

#include <print>
#include <ui/point_snapper.h>

int main()
{
    using namespace boost::ut;

    "point snapper"_test = [] {
        Kbi::Ui::PointSnapper snapper;
        expect(snapper.find(0.0, 0.0, 40.0) == nullptr) << "an empty snapper finds nothing";

        // The points are not sorted, the snapper sorts them.
        snapper.set({ { .x = 30.0, .y = 5.0, .index = 2 }, { .x = 10.0, .y = 5.0, .index = 0 },
            { .x = 20.0, .y = 5.0, .index = 1 } });

        const Kbi::Ui::SnapPoint* snapped = snapper.find(11.0, 5.0, 40.0);
        expect(snapped != nullptr) << "a point is found";
        if (snapped != nullptr) {
            expect(snapped->index == 0) << "the closest point is found";
            expect(snapped->x == 10.0) << "the position of the point is kept";
        }

        // A point that is too far away is not found.
        expect(snapper.find(500.0, 500.0, 40.0) == nullptr)
            << "a point outside of the radius is not found";

        // The closest point wins, not the first one.
        snapper.set({ { .x = 10.0, .y = 100.0, .index = 0 }, { .x = 12.0, .y = 0.0, .index = 1 } });
        snapped = snapper.find(11.0, 0.0, 40.0);
        expect(snapped != nullptr && snapped->index == 1) << "the closest point wins";

        // The radius is measured in pixels.
        snapper.set({ { .x = 10.0, .y = 0.0, .index = 0 } });
        expect(snapper.find(10.0, 39.0, 40.0) != nullptr) << "a point inside the radius is found";
        expect(snapper.find(10.0, 41.0, 40.0) == nullptr)
            << "a point outside the radius is not found";
    };
}
