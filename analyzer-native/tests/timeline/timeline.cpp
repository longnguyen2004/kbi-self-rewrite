#include <boost/ut.hpp>

#include <core/input_timeline.h>
#include <print>

int main()
{
    using namespace boost::ut;

    "input timeline"_test = [] {
        Kbi::InputTimeline timeline;
        timeline.add("kb", { .timestamp = 1000, .pressed = true, .code = 4 });
        timeline.add("kb", { .timestamp = 6000, .pressed = false, .code = 4 });
        expect(timeline.keypresses().size() == 1) << "one keypress is recorded";
        expect(timeline.keypresses()[0].start == 0.001) << "the keypress starts at the press";
        expect(timeline.keypresses()[0].end == 0.006) << "the keypress ends at the release";
        expect(timeline.keypresses()[0].key == "A") << "the keypress is named";
        expect(timeline.keys().size() == 1) << "the key set is updated";

        // A release without a matching press is ignored.
        timeline.add("kb", { .timestamp = 7000, .pressed = false, .code = 5 });
        expect(timeline.keypresses().size() == 1) << "unmatched releases are ignored";

        // The shortest keypress duration is the duration of the keypress.
        expect(timeline.shortestKeypressDuration() == 0.005)
            << "the shortest keypress duration is the duration of the keypress";

        // A second, shorter keypress is the new shortest.
        timeline.add("kb", { .timestamp = 7000, .pressed = true, .code = 5 });
        timeline.add("kb", { .timestamp = 10000, .pressed = false, .code = 5 });
        expect(timeline.keypresses().size() == 2) << "the second keypress is recorded";
        expect(timeline.shortestKeypressDuration() == 0.003)
            << "the shortest keypress duration is the minimum of the keypresses";
    };
}
