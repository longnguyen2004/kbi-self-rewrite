#include <boost/ut.hpp>

#include <parser/usb_descriptors.h>
#include <print>
#include <string>
#include <vector>

int main()
{
    using namespace boost::ut;

    "usb descriptors"_test = [] {
        // "Hello" -> "SGVsbG8="
        const std::string decoded = Kbi::base64Decode("SGVsbG8=");
        expect(decoded == "Hello") << "base64 decoding";

        // A device descriptor followed by a HID descriptor.
        const std::vector<unsigned char> raw
            = { 0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x31, 0x31, 0x50, 0x27, 0x00, 0x02,
                  0x01, 0x01, 0x03, 0x21, 0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x3f, 0x00 };
        const auto descriptors = Kbi::parseDescriptors(raw);
        expect(descriptors.size() == 2) << "the descriptor blob is split";
        expect(descriptors[0].typeName == "DEVICE") << "the device descriptor is recognized";
        expect(descriptors[1].typeName == "HID") << "the hid descriptor is recognized";
        expect(descriptors[0].fields.size() == 14) << "the device descriptor fields are decoded";
        expect(descriptors[0].fields[7].value == "0x3131") << "idVendor is decoded";
    };

    "usb polling rate"_test = [] {
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
        expect(descriptors.size() == 4) << "the descriptor blob is split";
        if (descriptors.size() < 4)
            return;

        // The USB version is added to the device descriptor.
        expect(descriptors[0].fields[2].value == "0200 (USB 2.0)")
            << "the usb version is annotated";

        const auto& interval = descriptors[2].fields[5];
        expect(interval.name == "bInterval") << "the endpoint interval is found";
        expect(interval.children.size() == 3) << "the polling rate is calculated";
        if (interval.children.size() == 3) {
            expect(interval.children[0].value == "1.25ms") << "the requested interval is formatted";
            expect(interval.children[1].value == "1ms") << "the effective interval is formatted";
            expect(interval.children[2].value == "1000Hz") << "the polling rate is calculated";
        }

        // A control endpoint has an interval of zero, which must not divide by
        // zero.
        const auto& controlInterval = descriptors[3].fields[5];
        expect(controlInterval.children.size() == 3) << "the control endpoint is decoded";
        if (controlInterval.children.size() == 3)
            expect(controlInterval.children[2].value == "InfinityHz")
                << "an interval of zero does not divide by zero";
    };
}
