#include <boost/ut.hpp>

#include <core/result.h>
#include <print>
#include <string>

int main()
{
    using namespace boost::ut;

    "result devices"_test = [] {
        Kbi::Result result;
        expect(result.devices.empty()) << "a new result has no devices";

        // The devices keep the order they were discovered in, the colors of the
        // input timeline are assigned in that order.
        result.addDevice("second", Kbi::ResultDevice { .name = "B" });
        result.addDevice("first", Kbi::ResultDevice { .name = "A" });
        expect(result.devices.size() == 2) << "the devices are added";
        expect(result.devices[0].first == "second" && result.devices[1].first == "first")
            << "the devices keep the order they were discovered in";

        // Adding a known device updates it.
        result.addDevice("second", Kbi::ResultDevice { .name = "B2" });
        expect(result.devices.size() == 2) << "a known device is not added twice";
        expect(result.findDevice("second") != nullptr && result.findDevice("second")->name == "B2")
            << "a known device is updated";
        expect(result.findDevice("missing") == nullptr) << "an unknown device is not found";
    };
}
