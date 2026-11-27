#include <boost/ut.hpp>

#include <core/result.h>
#include <parser/parser.h>
#include <print>
#include <string>

int main()
{
    using namespace boost::ut;

    "parser"_test = [] {
        const std::string path = std::string { KBI_TEST_DATA_DIR } + "/test_recording.json";

        const auto result = Kbi::parseResultFile(path);
        expect(result.has_value()) << "the recording is parsed";
        if (!result)
            return;
        expect(result->inputs.size() == 2) << "both devices are present";
        expect(result->devices.size() == 2) << "the device list is parsed";
        expect(result->usbDevices.size() == 1) << "the usb device list is parsed";
        expect(result->totalInputCount() == 1240) << "all input events are parsed";
        expect(result->info.backend == "evdev") << "the metadata is parsed";
    };

    "legacy kbi parser"_test = [] {
        const std::string path = std::string { KBI_TEST_DATA_DIR } + "/test_recording_legacy.kbi";

        const auto result = Kbi::parseResultFile(path);
        expect(result.has_value()) << "the legacy recording is parsed";
        if (!result)
            return;
        expect(result->inputs.size() == 2) << "both sources are present";
        expect(result->devices.size() == 2) << "the device list is parsed";
        const Kbi::ResultDevice* keyboardDevice = result->findDevice("HID_VID_3149&PID_5041&MI_00");
        expect(keyboardDevice != nullptr) << "the keyboard device is parsed";
        if (keyboardDevice != nullptr) {
            expect(keyboardDevice->name == "MonsGeek Keyboard") << "the device name is parsed";
            expect(keyboardDevice->vid == 0x3149) << "the vendor id is scraped from the device id";
            expect(keyboardDevice->pid == 0x5041) << "the product id is scraped from the device id";
        }
        expect(result->totalInputCount() == 8) << "all events are parsed";
        expect(result->time == "2025-07-22T22:34:43.000Z")
            << "the recording time is converted correctly (" + result->time + ")";

        const auto& keyboard = result->inputs.at("HID_VID_3149&PID_5041&MI_00");
        expect(keyboard.size() == 4) << "the keyboard events are grouped by source";
        expect(keyboard[0].timestamp == 0) << "the first timestamp is parsed";
        expect(keyboard[0].code == 0x04) << "the key name is mapped to a keycode";
        expect(!keyboard[1].pressed) << "the release event is parsed";
        expect(keyboard[2].code == 0x1E) << "the digit key name is mapped to a keycode";
        expect(result->inputs.at("HID_VID_3149&PID_5042&MI_00")[1].code == 0x3E)
            << "the function key name is mapped to a keycode";
    };
}
