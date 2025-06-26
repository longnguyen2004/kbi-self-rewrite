#include "wstring.h"
#include <windows.h>

std::wstring string_to_wstring(std::string_view str)
{
    int length = MultiByteToWideChar(CP_UTF8, 0, str.data(), str.size(), nullptr, 0);
    if (length == 0)
        return std::wstring();
    std::wstring wstr(length, '\0');
    MultiByteToWideChar(CP_UTF8, 0, str.data(), str.size(), wstr.data(), length);
    return wstr;
}

std::string wstring_to_string(std::wstring_view wstr)
{
    int length = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wstr.size(), nullptr, 0, nullptr, nullptr);
    if (length == 0)
        return std::string();
    std::string str(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), wstr.size(), str.data(), str.size(), nullptr, nullptr);
    return str;
}
