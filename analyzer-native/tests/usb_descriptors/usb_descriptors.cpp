#include <tests/tests.h>

#include <parser/usb_descriptors.h>
#include <print>
#include <string>
#include <vector>

namespace Kbi::Tests {
void testBase64()
{
    std::println("usb descriptors");
    // "Hello" -> "SGVsbG8="
    const std::string decoded = Kbi::base64Decode("SGVsbG8=");
    check(decoded == "Hello", "base64 decoding");

    // A device descriptor followed by a HID descriptor.
    const std::vector<unsigned char> raw
        = { 0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x31, 0x31, 0x50, 0x27, 0x00, 0x02,
              0x01, 0x01, 0x03, 0x21, 0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x3f, 0x00 };
    const auto descriptors = Kbi::parseDescriptors(raw);
    check(descriptors.size() == 2, "the descriptor blob is split");
    check(descriptors[0].typeName == "DEVICE", "the device descriptor is recognized");
    check(descriptors[1].typeName == "HID", "the hid descriptor is recognized");
    check(descriptors[0].fields.size() == 14, "the device descriptor fields are decoded");
    check(descriptors[0].fields[7].value == "0x3131", "idVendor is decoded");
}

void testUsbPollingRate()
{
    std::println("usb polling rate");

    // A device descriptor (USB 2.0) followed by an interrupt endpoint with a
    // bInterval of 10 and a control endpoint with a bInterval of 0.
    const std::vector<unsigned char> raw = {
        0x12,
        0x01,
        0x00,
        0x02,
        0x00,
        0x00,
        0x00,
        0x40,
        0x31,
        0x31,
        0x50,
        0x27,
        0x00,
        0x02,
        0x01,
        0x01,
        0x03,
        0x21,
        0x09,
        0x07,
        0x00,
        0x02,
        0x00,
        0x00,
        0x0a,
        0x00,
        0x00,
        0x07,
        0x05,
        0x81,
        0x03,
        0x08,
        0x00,
        0x0a,
        // A control endpoint, its interval is zero.
        0x07,
        0x05,
        0x80,
        0x02,
        0x08,
        0x00,
        0x00,
    };

    const auto descriptors = Kbi::parseUsbDescriptors(raw);
    check(descriptors.size() == 4, "the descriptor blob is split");
    if (descriptors.size() < 4)
        return;

    // The USB version is added to the device descriptor.
    check(descriptors[0].fields[2].value == "0200 (USB 2.0)", "the usb version is annotated");

    const auto& interval = descriptors[2].fields[5];
    check(interval.name == "bInterval", "the endpoint interval is found");
    check(interval.children.size() == 3, "the polling rate is calculated");
    if (interval.children.size() == 3) {
        check(interval.children[0].value == "1.25ms", "the requested interval is formatted");
        check(interval.children[1].value == "1ms", "the effective interval is formatted");
        check(interval.children[2].value == "1000Hz", "the polling rate is calculated");
    }

    // A control endpoint has an interval of zero, which must not divide by
    // zero.
    const auto& controlInterval = descriptors[3].fields[5];
    check(controlInterval.children.size() == 3, "the control endpoint is decoded");
    if (controlInterval.children.size() == 3)
        check(controlInterval.children[2].value == "InfinityHz",
            "an interval of zero does not divide by zero");
}

} // namespace Kbi::Tests
