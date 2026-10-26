#pragma once

#include <cstddef>
#include <format>
#include <string>
#include <vector>

namespace Kbi {

// Formats a byte sequence as space-separated two-digit hex, e.g. "09 0A FF".
[[nodiscard]] inline std::string hexBytes(const std::vector<unsigned char>& bytes)
{
    std::string out;
    out.reserve(bytes.size() * 3);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i != 0u)
            out += ' ';
        out += std::format("{:02X}", bytes[i]);
    }
    return out;
}

} // namespace Kbi
