#include <tests/tests.h>

#include <core/result.h>
#include <parser/parser.h>
#include <print>
#include <string>

namespace Kbi::Tests {
void testParser(const std::string& path)
{
    std::println("parser");
    const auto result = Kbi::parseResultFile(path);
    check(result.has_value(), "the recording is parsed");
    if (!result)
        return;
    check(result->inputs.size() == 2, "both devices are present");
    check(result->devices.size() == 2, "the device list is parsed");
    check(result->usbDevices.size() == 1, "the usb device list is parsed");
    check(result->totalInputCount() == 1240, "all input events are parsed");
    check(result->info.backend == "evdev", "the metadata is parsed");
}

void testLegacyParser(const std::string& path)
{
    std::println("legacy kbi parser");
    const auto result = Kbi::parseResultFile(path);
    check(result.has_value(), "the legacy recording is parsed");
    if (!result)
        return;
    check(result->inputs.size() == 2, "both sources are present");
    check(result->devices.size() == 2, "the device list is parsed");
    const Kbi::ResultDevice* keyboardDevice = result->findDevice("HID_VID_3149&PID_5041&MI_00");
    check(keyboardDevice != nullptr, "the keyboard device is parsed");
    if (keyboardDevice != nullptr) {
        check(keyboardDevice->name == "MonsGeek Keyboard", "the device name is parsed");
        check(keyboardDevice->vid == 0x3149, "the vendor id is scraped from the device id");
        check(keyboardDevice->pid == 0x5041, "the product id is scraped from the device id");
    }
    check(result->totalInputCount() == 8, "all events are parsed");
    check(result->time == "2025-07-22T22:34:43.000Z",
        "the recording time is converted correctly (" + result->time + ")");

    const auto& keyboard = result->inputs.at("HID_VID_3149&PID_5041&MI_00");
    check(keyboard.size() == 4, "the keyboard events are grouped by source");
    check(keyboard[0].timestamp == 0, "the first timestamp is parsed");
    check(keyboard[0].code == 0x04, "the key name is mapped to a keycode");
    check(!keyboard[1].pressed, "the release event is parsed");
    check(keyboard[2].code == 0x1E, "the digit key name is mapped to a keycode");
    check(result->inputs.at("HID_VID_3149&PID_5042&MI_00")[1].code == 0x3E,
        "the function key name is mapped to a keycode");
}

} // namespace Kbi::Tests
