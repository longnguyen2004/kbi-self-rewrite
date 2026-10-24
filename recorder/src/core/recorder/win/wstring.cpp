#include "wstring.h"
#include <simdutf.h>

namespace Kbi {

using namespace simdutf;

std::wstring stringToWstring(std::string_view str)
{
    auto length = utf16_length_from_utf8(str);
    if (length == 0)
        return {};
    std::wstring wstr(length, 0);
    convert_utf8_to_utf16(str.data(), str.size(), reinterpret_cast<char16_t*>(wstr.data()));
    return wstr;
}

std::string wstringToString(std::wstring_view wstr)
{
    auto length
        = utf8_length_from_utf16(reinterpret_cast<const char16_t*>(wstr.data()), wstr.size());
    if (length == 0)
        return {};
    std::string str(length, 0);
    convert_utf16_to_utf8(reinterpret_cast<const char16_t*>(wstr.data()), wstr.size(), str.data());
    return str;
}

} // namespace Kbi
