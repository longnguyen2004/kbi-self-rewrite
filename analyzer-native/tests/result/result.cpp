#include <tests/tests.h>

#include <core/result.h>
#include <print>
#include <string>

namespace Kbi::Tests {
void testResultDevices()
{
    std::println("result devices");
    Kbi::Result result;
    check(result.devices.empty(), "a new result has no devices");

    // The devices keep the order they were discovered in, the colors of the
    // input timeline are assigned in that order.
    result.addDevice("second", Kbi::ResultDevice { .name = "B" });
    result.addDevice("first", Kbi::ResultDevice { .name = "A" });
    check(result.devices.size() == 2, "the devices are added");
    check(result.devices[0].first == "second" && result.devices[1].first == "first",
        "the devices keep the order they were discovered in");

    // Adding a known device updates it.
    result.addDevice("second", Kbi::ResultDevice { .name = "B2" });
    check(result.devices.size() == 2, "a known device is not added twice");
    check(result.findDevice("second") != nullptr && result.findDevice("second")->name == "B2",
        "a known device is updated");
    check(result.findDevice("missing") == nullptr, "an unknown device is not found");
}

} // namespace Kbi::Tests
