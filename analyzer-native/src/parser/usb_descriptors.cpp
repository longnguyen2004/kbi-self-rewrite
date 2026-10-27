#include <core/hex_util.h>
#include <parser/usb_descriptors.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <map>
#include <simdutf.h>
#include <span>
#include <string_view>
#include <utility>

namespace Kbi {
namespace {

// Microseconds per second, used for the USB frame/polling-rate conversions.
constexpr double kMicrosPerSecond = 1'000'000.0;

std::string hexByte(unsigned value, int minLength = 2)
{
    return std::format("0x{:0{}X}", value, minLength);
}

std::string hex4(std::uint16_t value) { return std::format("{:04x}", value); }

std::uint16_t readU16(const std::vector<unsigned char>& raw, std::size_t offset)
{
    if (offset + 1 >= raw.size())
        return 0;
    return static_cast<std::uint16_t>(raw[offset] | (raw[offset + 1] << 8));
}

unsigned char readU8(const std::vector<unsigned char>& raw, std::size_t offset)
{
    if (offset >= raw.size())
        return 0;
    return raw[offset];
}

// Decodes UTF-16LE into UTF-8, including surrogate pairs.
std::string utf16leToUtf8(std::span<const unsigned char> data)
{
    const std::size_t units = data.size() / 2;
    std::vector<char16_t> utf16(units);
    for (std::ptrdiff_t i = 0; std::cmp_less(i, units); ++i)
        utf16[static_cast<std::size_t>(i)]
            = static_cast<char16_t>(data[static_cast<std::size_t>(i * 2)]
                | (data[static_cast<std::size_t>(i * 2 + 1)] << 8));

    const simdutf::result size
        = simdutf::utf8_length_from_utf16le_with_replacement(utf16.data(), units);
    if (size.error != simdutf::error_code::SUCCESS)
        return {};

    std::vector<char> utf8(size.count);
    const std::size_t written
        = simdutf::convert_utf16le_to_utf8_with_replacement(utf16.data(), units, utf8.data());
    return { utf8.data(), written };
}

const std::map<unsigned, std::string>& usbDescriptorTypes()
{
    static const std::map<unsigned, std::string> types = {
        { 0x01, "DEVICE" },
        { 0x02, "CONFIGURATION" },
        { 0x03, "STRING" },
        { 0x04, "INTERFACE" },
        { 0x05, "ENDPOINT" },
        { 0x06, "DEVICE_QUALIFIER" },
        { 0x07, "OTHER_SPEED_CONFIGURATION" },
        { 0x0b, "INTERFACE_ASSOCIATION" },
        { 0x0c, "HID" },
        { 0x0d, "REPORT" },
        { 0x0e, "PHYSICAL" },
        { 0x21, "HID" },
        { 0x22, "REPORT" },
        { 0x23, "PHYSICAL" },
        { 0x24, "HUB" },
        { 0x29, "HID_BUTTON" },
    };
    return types;
}

const std::map<unsigned, std::string>& usbClassCodes()
{
    static const std::map<unsigned, std::string> codes = {
        { 0x00, "Use class info from Interface Descriptors" },
        { 0x01, "Audio" },
        { 0x02, "Communications and CDC Control" },
        { 0x03, "HID (Human Interface Device)" },
        { 0x05, "Physical" },
        { 0x06, "Image" },
        { 0x07, "Printer" },
        { 0x08, "Mass Storage" },
        { 0x09, "Hub" },
        { 0x0a, "CDC Data" },
        { 0x0b, "Smart Card" },
        { 0x0d, "Content Security" },
        { 0x0e, "Video" },
        { 0x0f, "Personal Healthcare" },
        { 0x10, "Audio/Video Devices" },
        { 0x11, "Billboard Device Class" },
        { 0x12, "USB Type-C Bridge Class" },
        { 0xdc, "Diagnostic Device" },
        { 0xe0, "Wireless Controller" },
        { 0xef, "Miscellaneous" },
        { 0xfe, "Application Specific" },
        { 0xff, "Vendor Specific" },
    };
    return codes;
}

const std::map<unsigned, std::string>& endpointTransferTypes()
{
    static const std::map<unsigned, std::string> types = {
        { 0, "Control" },
        { 1, "Isochronous" },
        { 2, "Bulk" },
        { 3, "Interrupt" },
    };
    return types;
}

const std::map<unsigned, std::string>& endpointSyncTypes()
{
    static const std::map<unsigned, std::string> types = {
        { 0, "No Synchronization" },
        { 1, "Asynchronous" },
        { 2, "Adaptive" },
        { 3, "Synchronous" },
    };
    return types;
}

const std::map<unsigned, std::string>& endpointUsageTypes()
{
    static const std::map<unsigned, std::string> types = {
        { 0, "Data endpoint" },
        { 1, "Feedback endpoint" },
        { 2, "Implicit feedback Data endpoint" },
        { 3, "Reserved" },
    };
    return types;
}

const std::map<unsigned, std::string>& hidCountryCodes()
{
    static const std::map<unsigned, std::string> codes = {
        { 0x00, "Not supported" },
        { 0x01, "Arabic" },
        { 0x02, "Belgian" },
        { 0x03, "Canadian-Bilingual" },
        { 0x04, "Canadian-French" },
        { 0x05, "Czech Republic" },
        { 0x06, "Danish" },
        { 0x07, "Finnish" },
        { 0x08, "French" },
        { 0x09, "German" },
        { 0x0a, "Greek" },
        { 0x0b, "Hebrew" },
        { 0x0c, "Hungarian" },
        { 0x0d, "International (ISO)" },
        { 0x0e, "Italian" },
        { 0x0f, "Japan (Katakana)" },
        { 0x10, "Korean" },
        { 0x11, "Latin American" },
        { 0x12, "Netherlands/Dutch" },
        { 0x13, "Norwegian" },
        { 0x14, "Persian (Farsi)" },
        { 0x15, "Poland" },
        { 0x16, "Portuguese" },
        { 0x17, "Russia" },
        { 0x18, "Slovakia" },
        { 0x19, "Spanish" },
        { 0x1a, "Swedish" },
        { 0x1b, "Swiss/French" },
        { 0x1c, "Swiss/German" },
        { 0x1d, "Switzerland" },
        { 0x1e, "Taiwan" },
        { 0x1f, "Turkish-Q" },
        { 0x20, "UK" },
        { 0x21, "US" },
        { 0x22, "Yugoslavia" },
        { 0x23, "Turkish-F" },
    };
    return codes;
}

const std::map<unsigned, std::string>& hubCharPowerMode()
{
    static const std::map<unsigned, std::string> modes = {
        { 0, "Ganged power switching" },
        { 1, "Individual port power switching" },
    };
    return modes;
}

const std::map<unsigned, std::string>& hubCharCompound()
{
    static const std::map<unsigned, std::string> modes = {
        { 0, "Not a compound device" },
        { 1, "Compound device" },
    };
    return modes;
}

const std::map<unsigned, std::string>& hubCharOcMode()
{
    static const std::map<unsigned, std::string> modes = {
        { 0, "Global over-current protection" },
        { 1, "Individual port over-current protection" },
    };
    return modes;
}

// Formats a number the way JavaScript would, so the values match the ones
// the web analyzer displays.
std::string formatNumber(double value)
{
    if (std::isinf(value))
        return value > 0 ? "Infinity" : "-Infinity";
    if (std::isnan(value))
        return "NaN";
    // The default format uses the shortest representation that round trips.
    return std::format("{}", value);
}

std::string formatDuration(double microseconds)
{
    if (microseconds >= 1000.0)
        return std::format("{}ms", formatNumber(microseconds / 1000.0));
    return std::format("{}\u03bcs", formatNumber(microseconds));
}

std::string lookup(const std::map<unsigned, std::string>& table, unsigned key, unsigned fallback)
{
    const auto it = table.find(key);
    if (it != table.end())
        return it->second;
    return hexByte(fallback);
}

DescriptorField field(std::string name, std::string value)
{
    DescriptorField f;
    f.name = std::move(name);
    f.value = std::move(value);
    return f;
}

std::vector<DescriptorField> parseDeviceDescriptor(const std::vector<unsigned char>& raw)
{
    return {
        field("bLength", std::format("{}", readU8(raw, 0))),
        field("bDescriptorType", hexByte(readU8(raw, 1))),
        field("bcdUSB", hex4(readU16(raw, 2))),
        field("bDeviceClass", lookup(usbClassCodes(), readU8(raw, 4), readU8(raw, 4))),
        field("bDeviceSubClass", hexByte(readU8(raw, 5))),
        field("bDeviceProtocol", hexByte(readU8(raw, 6))),
        field("bMaxPacketSize0", std::format("{}", readU8(raw, 7))),
        field("idVendor", hexByte(readU16(raw, 8), 4)),
        field("idProduct", hexByte(readU16(raw, 10), 4)),
        field("bcdDevice", hex4(readU16(raw, 12))),
        field("iManufacturer", std::format("{}", readU8(raw, 14))),
        field("iProduct", std::format("{}", readU8(raw, 15))),
        field("iSerialNumber", std::format("{}", readU8(raw, 16))),
        field("bNumConfigurations", std::format("{}", readU8(raw, 17))),
    };
}

std::vector<DescriptorField> parseConfigurationDescriptor(const std::vector<unsigned char>& raw)
{
    return {
        field("bLength", std::format("{}", readU8(raw, 0))),
        field("bDescriptorType", hexByte(readU8(raw, 1))),
        field("wTotalLength", std::format("{}", readU16(raw, 2))),
        field("bNumInterfaces", std::format("{}", readU8(raw, 4))),
        field("bConfigurationValue", std::format("{}", readU8(raw, 5))),
        field("iConfiguration", std::format("{}", readU8(raw, 6))),
        field("bmAttributes", hexByte(readU8(raw, 7))),
        field("bMaxPower", std::format("{} mA", readU8(raw, 8) * 2)),
    };
}

std::vector<DescriptorField> parseInterfaceDescriptor(const std::vector<unsigned char>& raw)
{
    return {
        field("bLength", std::format("{}", readU8(raw, 0))),
        field("bDescriptorType", hexByte(readU8(raw, 1))),
        field("bInterfaceNumber", std::format("{}", readU8(raw, 2))),
        field("bAlternateSetting", std::format("{}", readU8(raw, 3))),
        field("bNumEndpoints", std::format("{}", readU8(raw, 4))),
        field("bInterfaceClass", lookup(usbClassCodes(), readU8(raw, 5), readU8(raw, 5))),
        field("bInterfaceSubClass", hexByte(readU8(raw, 6))),
        field("bInterfaceProtocol", hexByte(readU8(raw, 7))),
        field("iInterface", std::format("{}", readU8(raw, 8))),
    };
}

std::vector<DescriptorField> parseEndpointDescriptor(const std::vector<unsigned char>& raw)
{
    const unsigned char epAddr = readU8(raw, 2);
    const unsigned epNum = epAddr & 0x0f;
    const std::string_view epDir = ((epAddr & 0x80) != 0) ? "IN" : "OUT";
    const unsigned char attr = readU8(raw, 3);
    const unsigned transferType = attr & 0x03;
    const unsigned syncType = (attr >> 2) & 0x03;
    const unsigned usageType = (attr >> 4) & 0x03;

    DescriptorField attributes = field("bmAttributes", hexByte(attr));
    attributes.children = {
        field("Transfer Type", lookup(endpointTransferTypes(), transferType, transferType)),
        field("Sync Type", lookup(endpointSyncTypes(), syncType, syncType)),
        field("Usage Type", lookup(endpointUsageTypes(), usageType, usageType)),
    };

    return {
        field("bLength", std::format("{}", readU8(raw, 0))),
        field("bDescriptorType", hexByte(readU8(raw, 1))),
        field(
            "bEndpointAddress", std::format("{} (Endpoint {}, {})", hexByte(epAddr), epNum, epDir)),
        std::move(attributes),
        field("wMaxPacketSize", std::format("{}", readU16(raw, 4))),
        field("bInterval", std::format("{}", readU8(raw, 6))),
    };
}

std::vector<DescriptorField> parseHidDescriptor(const std::vector<unsigned char>& raw)
{
    std::vector<DescriptorField> subDescriptors;
    const unsigned char numDescriptors = readU8(raw, 5);

    std::size_t offset = 6;
    for (unsigned i = 0; i < numDescriptors && offset + 3 <= raw.size(); ++i) {
        const unsigned char subType = readU8(raw, offset);
        const std::uint16_t subLen = readU16(raw, offset + 1);
        const auto typeIt = usbDescriptorTypes().find(subType);
        const std::string typeName
            = typeIt != usbDescriptorTypes().end() ? typeIt->second : hexByte(subType);
        subDescriptors.push_back(field(std::format("Sub-descriptor {}", i + 1),
            std::format("{} ({} bytes)", typeName, subLen)));
        offset += 3;
    }

    DescriptorField count = field("bNumDescriptors", std::format("{}", numDescriptors));
    count.children = std::move(subDescriptors);

    return {
        field("bLength", std::format("{}", readU8(raw, 0))),
        field("bDescriptorType", hexByte(readU8(raw, 1))),
        field("bcdHID", hex4(readU16(raw, 2))),
        field("bCountryCode", lookup(hidCountryCodes(), readU8(raw, 4), readU8(raw, 4))),
        std::move(count),
    };
}

std::vector<DescriptorField> parseHubDescriptor(const std::vector<unsigned char>& raw)
{
    const std::uint16_t chars = readU16(raw, 3);
    const unsigned powerMode = chars & 0x03;
    const unsigned compound = (chars >> 2) & 0x01;
    const unsigned ocMode = (chars >> 3) & 0x01;

    DescriptorField characteristics = field("wHubCharacteristics", hexByte(chars));
    characteristics.children = {
        field("Power Switching", lookup(hubCharPowerMode(), powerMode, powerMode)),
        field("Compound Device", lookup(hubCharCompound(), compound, compound)),
        field("Over-current Protection", lookup(hubCharOcMode(), ocMode, ocMode)),
    };

    return {
        field("bLength", std::format("{}", readU8(raw, 0))),
        field("bDescriptorType", hexByte(readU8(raw, 1))),
        field("bNbrPorts", std::format("{}", readU8(raw, 2))),
        std::move(characteristics),
        field("bPwrOn2PwrGood", std::format("{} ms", readU8(raw, 5) * 2)),
        field("bHubContrCurrent", std::format("{} mA", readU8(raw, 6))),
    };
}

std::vector<DescriptorField> parseInterfaceAssociationDescriptor(
    const std::vector<unsigned char>& raw)
{
    return {
        field("bLength", std::format("{}", readU8(raw, 0))),
        field("bDescriptorType", hexByte(readU8(raw, 1))),
        field("bFirstInterface", std::format("{}", readU8(raw, 2))),
        field("bInterfaceCount", std::format("{}", readU8(raw, 3))),
        field("bFunctionClass", lookup(usbClassCodes(), readU8(raw, 4), readU8(raw, 4))),
        field("bFunctionSubClass", hexByte(readU8(raw, 5))),
        field("bFunctionProtocol", hexByte(readU8(raw, 6))),
        field("iFunction", std::format("{}", readU8(raw, 7))),
    };
}

std::vector<DescriptorField> parseStringDescriptor(const std::vector<unsigned char>& raw)
{
    std::vector<DescriptorField> fields = {
        field("bLength", std::format("{}", readU8(raw, 0))),
        field("bDescriptorType", hexByte(readU8(raw, 1))),
    };
    if (raw.size() > 2) {
        const std::vector<unsigned char> payload(raw.begin() + 2, raw.end());
        fields.push_back(field("bString", utf16leToUtf8(payload)));
    }
    return fields;
}

std::vector<DescriptorField> parseGenericDescriptor(const std::vector<unsigned char>& raw)
{
    std::vector<DescriptorField> fields = {
        field("bLength", std::format("{}", readU8(raw, 0))),
        field("bDescriptorType", hexByte(readU8(raw, 1))),
    };
    if (raw.size() > 2) {
        const std::vector<unsigned char> payload(raw.begin() + 2, raw.end());
        fields.push_back(field("Data", hexBytes(payload)));
    }
    return fields;
}

std::vector<DescriptorField> parseDescriptorFields(const std::vector<unsigned char>& raw)
{
    switch (readU8(raw, 1)) {
    case 0x01:
        return parseDeviceDescriptor(raw);
    case 0x02:
        return parseConfigurationDescriptor(raw);
    case 0x03:
        return parseStringDescriptor(raw);
    case 0x04:
        return parseInterfaceDescriptor(raw);
    case 0x05:
        return parseEndpointDescriptor(raw);
    case 0x0b:
        return parseInterfaceAssociationDescriptor(raw);
    case 0x21:
        return parseHidDescriptor(raw);
    case 0x24:
        return parseHubDescriptor(raw);
    default:
        return parseGenericDescriptor(raw);
    }
}

// The largest power of two that is not larger than the value.
std::uint16_t roundDownToPowerOfTwo(std::uint16_t x)
{
    return static_cast<std::uint16_t>(std::bit_floor(static_cast<unsigned>(x)));
}

} // namespace

std::string base64Decode(const std::string& base64)
{
    const std::size_t maxLength
        = simdutf::maximal_binary_length_from_base64(base64.data(), base64.size());
    std::vector<char> buffer(maxLength);
    const simdutf::result result
        = simdutf::base64_to_binary(base64.data(), base64.size(), buffer.data());
    if (result.error != simdutf::error_code::SUCCESS)
        return {};
    return { buffer.data(), result.count };
}

std::vector<ParsedDescriptor> parseDescriptors(const std::vector<unsigned char>& raw)
{
    std::vector<ParsedDescriptor> descriptors;
    std::size_t offset = 0;
    std::size_t index = 0;

    while (offset < raw.size()) {
        const std::size_t length = readU8(raw, offset);
        if (length == 0 || offset + length > raw.size())
            break;
        const unsigned char type = readU8(raw, offset + 1);
        const std::vector<unsigned char> slice(raw.begin() + static_cast<long>(offset),
            raw.begin() + static_cast<long>(offset + length));

        ParsedDescriptor descriptor;
        descriptor.index = index;
        descriptor.type = type;
        const auto typeIt = usbDescriptorTypes().find(type);
        descriptor.typeName = typeIt != usbDescriptorTypes().end() ? typeIt->second : hexByte(type);
        descriptor.length = length;
        descriptor.raw = slice;
        descriptor.fields = parseDescriptorFields(slice);
        descriptors.push_back(std::move(descriptor));

        offset += length;
        ++index;
    }

    return descriptors;
}

std::vector<ParsedDescriptor> parseUsbDescriptorsBase64(const std::string& base64)
{
    const std::string decoded = base64Decode(base64);
    const std::vector<unsigned char> raw(decoded.begin(), decoded.end());
    return parseUsbDescriptors(raw);
}

std::vector<ParsedDescriptor> parseUsbDescriptors(const std::vector<unsigned char>& raw)
{
    auto descriptors = parseDescriptors(raw);

    int usbMajorVersion = 0;
    for (auto& descriptor : descriptors) {
        switch (descriptor.type) {
        case 0x01: {
            // Device descriptor: annotate the USB version.
            for (auto& f : descriptor.fields) {
                if (f.name != "bcdUSB" || f.value.size() < 4)
                    continue;
                usbMajorVersion = f.value[1] - '0';
                const int usbMinorVersion = f.value[2] - '0';
                f.value += std::format(" (USB {}.{})", usbMajorVersion, usbMinorVersion);
            }
        } break;
        case 0x05: {
            // Endpoint descriptor: derive the polling rate.
            if (usbMajorVersion == 0)
                break;
            // USB 1.x has 1ms frames, USB 2.0+ has 125us microframes.
            const unsigned frameDuration = usbMajorVersion == 1 ? 1000 : 125;
            for (auto& f : descriptor.fields) {
                if (f.name != "bInterval")
                    continue;
                unsigned requested = 0;
                for (const char c : f.value) {
                    if (c < '0' || c > '9')
                        break;
                    requested = requested * 10 + static_cast<unsigned>(c - '0');
                }
                const unsigned effective
                    = roundDownToPowerOfTwo(static_cast<std::uint16_t>(requested));
                f.children = {
                    field("Polling Interval (Requested)",
                        formatDuration(static_cast<double>(requested) * frameDuration)),
                    field("Polling Interval (Effective)",
                        formatDuration(static_cast<double>(effective) * frameDuration)),
                    field("Polling Rate",
                        formatNumber(kMicrosPerSecond / static_cast<double>(effective)
                            / static_cast<double>(frameDuration))
                            + "Hz"),
                };
            }
        } break;
        default:
            break;
        }
    }

    return descriptors;
}

} // namespace Kbi
