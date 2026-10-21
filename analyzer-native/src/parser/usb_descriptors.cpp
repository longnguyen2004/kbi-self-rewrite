#include "parser/usb_descriptors.h"

#include <algorithm>
#include <bit>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <format>
#include <map>
#include <simdutf.h>

namespace kbi {
namespace {

std::string hex_byte(unsigned value, int min_length = 2) {
    return std::format("0x{:0{}X}", value, min_length);
}

std::string hex_bytes(const std::vector<unsigned char>& bytes) {
    std::string out;
    out.reserve(bytes.size() * 3);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i)
            out += ' ';
        out += std::format("{:02X}", bytes[i]);
    }
    return out;
}

std::string hex4(std::uint16_t value) {
    return std::format("{:04x}", value);
}

std::uint16_t read_u16(const std::vector<unsigned char>& raw, std::size_t offset) {
    if (offset + 1 >= raw.size())
        return 0;
    return static_cast<std::uint16_t>(raw[offset] | (raw[offset + 1] << 8));
}

unsigned char read_u8(const std::vector<unsigned char>& raw, std::size_t offset) {
    if (offset >= raw.size())
        return 0;
    return raw[offset];
}

// Decodes UTF-16LE into UTF-8, including surrogate pairs.
std::string utf16le_to_utf8(const unsigned char* data, std::size_t length) {
    const std::size_t units = length / 2;
    std::vector<char16_t> utf16(units);
    for (std::size_t i = 0; i < units; ++i)
        utf16[i] = static_cast<char16_t>(data[i * 2] | (data[i * 2 + 1] << 8));

    const simdutf::result size = simdutf::utf8_length_from_utf16le_with_replacement(
        utf16.data(), units);
    if (size.error != simdutf::error_code::SUCCESS)
        return {};

    std::vector<char> utf8(size.count);
    const std::size_t written = simdutf::convert_utf16le_to_utf8_with_replacement(
        utf16.data(), units, utf8.data());
    return std::string(utf8.data(), written);
}

const std::map<unsigned, std::string>& usb_descriptor_types() {
    static const std::map<unsigned, std::string> types = {
        {0x01, "DEVICE"},          {0x02, "CONFIGURATION"},
        {0x03, "STRING"},         {0x04, "INTERFACE"},
        {0x05, "ENDPOINT"},        {0x06, "DEVICE_QUALIFIER"},
        {0x07, "OTHER_SPEED_CONFIGURATION"}, {0x0b, "INTERFACE_ASSOCIATION"},
        {0x0c, "HID"},             {0x0d, "REPORT"},
        {0x0e, "PHYSICAL"},        {0x21, "HID"},
        {0x22, "REPORT"},          {0x23, "PHYSICAL"},
        {0x24, "HUB"},             {0x29, "HID_BUTTON"},
    };
    return types;
}

const std::map<unsigned, std::string>& usb_class_codes() {
    static const std::map<unsigned, std::string> codes = {
        {0x00, "Use class info from Interface Descriptors"},
        {0x01, "Audio"},
        {0x02, "Communications and CDC Control"},
        {0x03, "HID (Human Interface Device)"},
        {0x05, "Physical"},
        {0x06, "Image"},
        {0x07, "Printer"},
        {0x08, "Mass Storage"},
        {0x09, "Hub"},
        {0x0a, "CDC Data"},
        {0x0b, "Smart Card"},
        {0x0d, "Content Security"},
        {0x0e, "Video"},
        {0x0f, "Personal Healthcare"},
        {0x10, "Audio/Video Devices"},
        {0x11, "Billboard Device Class"},
        {0x12, "USB Type-C Bridge Class"},
        {0xdc, "Diagnostic Device"},
        {0xe0, "Wireless Controller"},
        {0xef, "Miscellaneous"},
        {0xfe, "Application Specific"},
        {0xff, "Vendor Specific"},
    };
    return codes;
}

const std::map<unsigned, std::string>& endpoint_transfer_types() {
    static const std::map<unsigned, std::string> types = {
        {0, "Control"}, {1, "Isochronous"}, {2, "Bulk"}, {3, "Interrupt"},
    };
    return types;
}

const std::map<unsigned, std::string>& endpoint_sync_types() {
    static const std::map<unsigned, std::string> types = {
        {0, "No Synchronization"}, {1, "Asynchronous"}, {2, "Adaptive"}, {3, "Synchronous"},
    };
    return types;
}

const std::map<unsigned, std::string>& endpoint_usage_types() {
    static const std::map<unsigned, std::string> types = {
        {0, "Data endpoint"},
        {1, "Feedback endpoint"},
        {2, "Implicit feedback Data endpoint"},
        {3, "Reserved"},
    };
    return types;
}

const std::map<unsigned, std::string>& hid_country_codes() {
    static const std::map<unsigned, std::string> codes = {
        {0x00, "Not supported"},       {0x01, "Arabic"},
        {0x02, "Belgian"},             {0x03, "Canadian-Bilingual"},
        {0x04, "Canadian-French"},     {0x05, "Czech Republic"},
        {0x06, "Danish"},              {0x07, "Finnish"},
        {0x08, "French"},              {0x09, "German"},
        {0x0a, "Greek"},               {0x0b, "Hebrew"},
        {0x0c, "Hungarian"},           {0x0d, "International (ISO)"},
        {0x0e, "Italian"},             {0x0f, "Japan (Katakana)"},
        {0x10, "Korean"},              {0x11, "Latin American"},
        {0x12, "Netherlands/Dutch"},   {0x13, "Norwegian"},
        {0x14, "Persian (Farsi)"},     {0x15, "Poland"},
        {0x16, "Portuguese"},          {0x17, "Russia"},
        {0x18, "Slovakia"},            {0x19, "Spanish"},
        {0x1a, "Swedish"},             {0x1b, "Swiss/French"},
        {0x1c, "Swiss/German"},        {0x1d, "Switzerland"},
        {0x1e, "Taiwan"},              {0x1f, "Turkish-Q"},
        {0x20, "UK"},                  {0x21, "US"},
        {0x22, "Yugoslavia"},          {0x23, "Turkish-F"},
    };
    return codes;
}

const std::map<unsigned, std::string>& hub_char_power_mode() {
    static const std::map<unsigned, std::string> modes = {
        {0, "Ganged power switching"},
        {1, "Individual port power switching"},
    };
    return modes;
}

const std::map<unsigned, std::string>& hub_char_compound() {
    static const std::map<unsigned, std::string> modes = {
        {0, "Not a compound device"},
        {1, "Compound device"},
    };
    return modes;
}

const std::map<unsigned, std::string>& hub_char_oc_mode() {
    static const std::map<unsigned, std::string> modes = {
        {0, "Global over-current protection"},
        {1, "Individual port over-current protection"},
    };
    return modes;
}

// Formats a number the way JavaScript would, so the values match the ones
// the web analyzer displays.
std::string format_number(double value) {
    if (std::isinf(value))
        return value > 0 ? "Infinity" : "-Infinity";
    if (std::isnan(value))
        return "NaN";
    // The default format uses the shortest representation that round trips.
    return std::format("{}", value);
}

std::string format_duration(double microseconds) {
    if (microseconds >= 1000.0)
        return std::format("{}ms", format_number(microseconds / 1000.0));
    return std::format("{}\u03bcs", format_number(microseconds));
}

std::string lookup(const std::map<unsigned, std::string>& table, unsigned key, unsigned fallback) {
    const auto it = table.find(key);
    if (it != table.end())
        return it->second;
    return hex_byte(fallback);
}

DescriptorField field(std::string name, std::string value) {
    DescriptorField f;
    f.name = std::move(name);
    f.value = std::move(value);
    return f;
}

std::vector<DescriptorField> parse_device_descriptor(const std::vector<unsigned char>& raw) {
    return {
        field("bLength", std::format("{}", read_u8(raw, 0))),
        field("bDescriptorType", hex_byte(read_u8(raw, 1))),
        field("bcdUSB", hex4(read_u16(raw, 2))),
        field("bDeviceClass", lookup(usb_class_codes(), read_u8(raw, 4), read_u8(raw, 4))),
        field("bDeviceSubClass", hex_byte(read_u8(raw, 5))),
        field("bDeviceProtocol", hex_byte(read_u8(raw, 6))),
        field("bMaxPacketSize0", std::format("{}", read_u8(raw, 7))),
        field("idVendor", hex_byte(read_u16(raw, 8), 4)),
        field("idProduct", hex_byte(read_u16(raw, 10), 4)),
        field("bcdDevice", hex4(read_u16(raw, 12))),
        field("iManufacturer", std::format("{}", read_u8(raw, 14))),
        field("iProduct", std::format("{}", read_u8(raw, 15))),
        field("iSerialNumber", std::format("{}", read_u8(raw, 16))),
        field("bNumConfigurations", std::format("{}", read_u8(raw, 17))),
    };
}

std::vector<DescriptorField> parse_configuration_descriptor(const std::vector<unsigned char>& raw) {
    return {
        field("bLength", std::format("{}", read_u8(raw, 0))),
        field("bDescriptorType", hex_byte(read_u8(raw, 1))),
        field("wTotalLength", std::format("{}", read_u16(raw, 2))),
        field("bNumInterfaces", std::format("{}", read_u8(raw, 4))),
        field("bConfigurationValue", std::format("{}", read_u8(raw, 5))),
        field("iConfiguration", std::format("{}", read_u8(raw, 6))),
        field("bmAttributes", hex_byte(read_u8(raw, 7))),
        field("bMaxPower", std::format("{} mA", read_u8(raw, 8) * 2)),
    };
}

std::vector<DescriptorField> parse_interface_descriptor(const std::vector<unsigned char>& raw) {
    return {
        field("bLength", std::format("{}", read_u8(raw, 0))),
        field("bDescriptorType", hex_byte(read_u8(raw, 1))),
        field("bInterfaceNumber", std::format("{}", read_u8(raw, 2))),
        field("bAlternateSetting", std::format("{}", read_u8(raw, 3))),
        field("bNumEndpoints", std::format("{}", read_u8(raw, 4))),
        field("bInterfaceClass", lookup(usb_class_codes(), read_u8(raw, 5), read_u8(raw, 5))),
        field("bInterfaceSubClass", hex_byte(read_u8(raw, 6))),
        field("bInterfaceProtocol", hex_byte(read_u8(raw, 7))),
        field("iInterface", std::format("{}", read_u8(raw, 8))),
    };
}

std::vector<DescriptorField> parse_endpoint_descriptor(const std::vector<unsigned char>& raw) {
    const unsigned char ep_addr = read_u8(raw, 2);
    const unsigned ep_num = ep_addr & 0x0f;
    const char* ep_dir = (ep_addr & 0x80) ? "IN" : "OUT";
    const unsigned char attr = read_u8(raw, 3);
    const unsigned transfer_type = attr & 0x03;
    const unsigned sync_type = (attr >> 2) & 0x03;
    const unsigned usage_type = (attr >> 4) & 0x03;

    DescriptorField attributes = field("bmAttributes", hex_byte(attr));
    attributes.children = {
        field("Transfer Type", lookup(endpoint_transfer_types(), transfer_type, transfer_type)),
        field("Sync Type", lookup(endpoint_sync_types(), sync_type, sync_type)),
        field("Usage Type", lookup(endpoint_usage_types(), usage_type, usage_type)),
    };

    return {
        field("bLength", std::format("{}", read_u8(raw, 0))),
        field("bDescriptorType", hex_byte(read_u8(raw, 1))),
        field("bEndpointAddress",
              std::format("{} (Endpoint {}, {})", hex_byte(ep_addr), ep_num, ep_dir)),
        std::move(attributes),
        field("wMaxPacketSize", std::format("{}", read_u16(raw, 4))),
        field("bInterval", std::format("{}", read_u8(raw, 6))),
    };
}

std::vector<DescriptorField> parse_hid_descriptor(const std::vector<unsigned char>& raw) {
    std::vector<DescriptorField> sub_descriptors;
    const unsigned char num_descriptors = read_u8(raw, 5);

    std::size_t offset = 6;
    for (unsigned i = 0; i < num_descriptors && offset + 3 <= raw.size(); ++i) {
        const unsigned char sub_type = read_u8(raw, offset);
        const std::uint16_t sub_len = read_u16(raw, offset + 1);
        const auto type_it = usb_descriptor_types().find(sub_type);
        const std::string type_name = type_it != usb_descriptor_types().end()
                                          ? type_it->second
                                          : hex_byte(sub_type);
        sub_descriptors.push_back(field(std::format("Sub-descriptor {}", i + 1),
                                        std::format("{} ({} bytes)", type_name, sub_len)));
        offset += 3;
    }

    DescriptorField count = field("bNumDescriptors", std::format("{}", num_descriptors));
    count.children = std::move(sub_descriptors);

    return {
        field("bLength", std::format("{}", read_u8(raw, 0))),
        field("bDescriptorType", hex_byte(read_u8(raw, 1))),
        field("bcdHID", hex4(read_u16(raw, 2))),
        field("bCountryCode", lookup(hid_country_codes(), read_u8(raw, 4), read_u8(raw, 4))),
        std::move(count),
    };
}

std::vector<DescriptorField> parse_hub_descriptor(const std::vector<unsigned char>& raw) {
    const std::uint16_t chars = read_u16(raw, 3);
    const unsigned power_mode = chars & 0x03;
    const unsigned compound = (chars >> 2) & 0x01;
    const unsigned oc_mode = (chars >> 3) & 0x01;

    DescriptorField characteristics = field("wHubCharacteristics", hex_byte(chars));
    characteristics.children = {
        field("Power Switching", lookup(hub_char_power_mode(), power_mode, power_mode)),
        field("Compound Device", lookup(hub_char_compound(), compound, compound)),
        field("Over-current Protection", lookup(hub_char_oc_mode(), oc_mode, oc_mode)),
    };

    return {
        field("bLength", std::format("{}", read_u8(raw, 0))),
        field("bDescriptorType", hex_byte(read_u8(raw, 1))),
        field("bNbrPorts", std::format("{}", read_u8(raw, 2))),
        std::move(characteristics),
        field("bPwrOn2PwrGood", std::format("{} ms", read_u8(raw, 5) * 2)),
        field("bHubContrCurrent", std::format("{} mA", read_u8(raw, 6))),
    };
}

std::vector<DescriptorField> parse_interface_association_descriptor(
    const std::vector<unsigned char>& raw) {
    return {
        field("bLength", std::format("{}", read_u8(raw, 0))),
        field("bDescriptorType", hex_byte(read_u8(raw, 1))),
        field("bFirstInterface", std::format("{}", read_u8(raw, 2))),
        field("bInterfaceCount", std::format("{}", read_u8(raw, 3))),
        field("bFunctionClass", lookup(usb_class_codes(), read_u8(raw, 4), read_u8(raw, 4))),
        field("bFunctionSubClass", hex_byte(read_u8(raw, 5))),
        field("bFunctionProtocol", hex_byte(read_u8(raw, 6))),
        field("iFunction", std::format("{}", read_u8(raw, 7))),
    };
}

std::vector<DescriptorField> parse_string_descriptor(const std::vector<unsigned char>& raw) {
    std::vector<DescriptorField> fields = {
        field("bLength", std::format("{}", read_u8(raw, 0))),
        field("bDescriptorType", hex_byte(read_u8(raw, 1))),
    };
    if (raw.size() > 2) {
        const std::vector<unsigned char> payload(raw.begin() + 2, raw.end());
        fields.push_back(field("bString", utf16le_to_utf8(payload.data(), payload.size())));
    }
    return fields;
}

std::vector<DescriptorField> parse_generic_descriptor(const std::vector<unsigned char>& raw) {
    std::vector<DescriptorField> fields = {
        field("bLength", std::format("{}", read_u8(raw, 0))),
        field("bDescriptorType", hex_byte(read_u8(raw, 1))),
    };
    if (raw.size() > 2) {
        const std::vector<unsigned char> payload(raw.begin() + 2, raw.end());
        fields.push_back(field("Data", hex_bytes(payload)));
    }
    return fields;
}

std::vector<DescriptorField> parse_descriptor_fields(const std::vector<unsigned char>& raw) {
    switch (read_u8(raw, 1)) {
    case 0x01:
        return parse_device_descriptor(raw);
    case 0x02:
        return parse_configuration_descriptor(raw);
    case 0x03:
        return parse_string_descriptor(raw);
    case 0x04:
        return parse_interface_descriptor(raw);
    case 0x05:
        return parse_endpoint_descriptor(raw);
    case 0x0b:
        return parse_interface_association_descriptor(raw);
    case 0x21:
        return parse_hid_descriptor(raw);
    case 0x24:
        return parse_hub_descriptor(raw);
    default:
        return parse_generic_descriptor(raw);
    }
}

// The largest power of two that is not larger than the value.
std::uint16_t round_down_to_power_of_two(std::uint16_t x) {
    return static_cast<std::uint16_t>(std::bit_floor(static_cast<unsigned>(x)));
}

} // namespace

std::string base64_decode(const std::string& base64) {
    const std::size_t max_length =
        simdutf::maximal_binary_length_from_base64(base64.data(), base64.size());
    std::vector<char> buffer(max_length);
    const simdutf::result result =
        simdutf::base64_to_binary(base64.data(), base64.size(), buffer.data());
    if (result.error != simdutf::error_code::SUCCESS)
        return {};
    return std::string(buffer.data(), result.count);
}

std::vector<ParsedDescriptor> parse_descriptors(const std::vector<unsigned char>& raw) {
    std::vector<ParsedDescriptor> descriptors;
    std::size_t offset = 0;
    std::size_t index = 0;

    while (offset < raw.size()) {
        const std::size_t length = read_u8(raw, offset);
        if (length == 0 || offset + length > raw.size())
            break;
        const unsigned char type = read_u8(raw, offset + 1);
        const std::vector<unsigned char> slice(raw.begin() + static_cast<long>(offset),
                                               raw.begin() + static_cast<long>(offset + length));

        ParsedDescriptor descriptor;
        descriptor.index = index;
        descriptor.type = type;
        const auto type_it = usb_descriptor_types().find(type);
        descriptor.type_name = type_it != usb_descriptor_types().end()
                                   ? type_it->second
                                   : hex_byte(type);
        descriptor.length = length;
        descriptor.raw = slice;
        descriptor.fields = parse_descriptor_fields(slice);
        descriptors.push_back(std::move(descriptor));

        offset += length;
        ++index;
    }

    return descriptors;
}

std::vector<ParsedDescriptor> parse_usb_descriptors_base64(const std::string& base64) {
    const std::string decoded = base64_decode(base64);
    const std::vector<unsigned char> raw(decoded.begin(), decoded.end());
    return parse_usb_descriptors(raw);
}

std::vector<ParsedDescriptor> parse_usb_descriptors(const std::vector<unsigned char>& raw) {
    auto descriptors = parse_descriptors(raw);

    int usb_major_version = 0;
    for (auto& descriptor : descriptors) {
        switch (descriptor.type) {
        case 0x01: {
            // Device descriptor: annotate the USB version.
            for (auto& f : descriptor.fields) {
                if (f.name != "bcdUSB" || f.value.size() < 4)
                    continue;
                usb_major_version = f.value[1] - '0';
                const int usb_minor_version = f.value[2] - '0';
                f.value += std::format(" (USB {}.{})", usb_major_version, usb_minor_version);
            }
        } break;
        case 0x05: {
            // Endpoint descriptor: derive the polling rate.
            if (usb_major_version == 0)
                break;
            // USB 1.x has 1ms frames, USB 2.0+ has 125us microframes.
            const unsigned frame_duration = usb_major_version == 1 ? 1000 : 125;
            for (auto& f : descriptor.fields) {
                if (f.name != "bInterval")
                    continue;
                unsigned requested = 0;
                for (const char c : f.value) {
                    if (c < '0' || c > '9')
                        break;
                    requested = requested * 10 + static_cast<unsigned>(c - '0');
                }
                const unsigned effective = round_down_to_power_of_two(
                    static_cast<std::uint16_t>(requested));
                f.children = {
                    field("Polling Interval (Requested)",
                          format_duration(static_cast<double>(requested) * frame_duration)),
                    field("Polling Interval (Effective)",
                          format_duration(static_cast<double>(effective) * frame_duration)),
                    field("Polling Rate",
                          format_number(1000000.0 / static_cast<double>(effective) /
                                        static_cast<double>(frame_duration)) +
                              "Hz"),
                };
            }
        } break;
        default:
            break;
        }
    }

    return descriptors;
}

} // namespace kbi
