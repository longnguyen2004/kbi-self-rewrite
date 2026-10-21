#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace kbi {

struct DescriptorField {
    std::string name;
    std::string value;
    std::vector<DescriptorField> children;
};

struct ParsedDescriptor {
    std::size_t index = 0;
    std::uint8_t type = 0;
    std::string type_name;
    std::size_t length = 0;
    std::vector<unsigned char> raw;
    std::vector<DescriptorField> fields;
};

// Parses a raw USB descriptor blob (the contents of a device's
// `descriptors` sysfs file).
std::vector<ParsedDescriptor> parse_descriptors(const std::vector<unsigned char>& raw);

// Same as `parse_descriptors`, but also annotates the USB version and derives
// the endpoint polling rates.
std::vector<ParsedDescriptor> parse_usb_descriptors(const std::vector<unsigned char>& raw);

// Parses and post-processes a base64-encoded descriptor blob.
std::vector<ParsedDescriptor> parse_usb_descriptors_base64(const std::string& base64);

std::string base64_decode(const std::string& base64);

} // namespace kbi
