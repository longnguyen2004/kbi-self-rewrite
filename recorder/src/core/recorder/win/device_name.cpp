#include "device_name.h"
#include <wil/resource.h>
#include <wil/result.h>
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <charconv>

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

std::string device_name_from_pnp(std::string_view id)
{
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    auto wid = string_to_wstring(id);
    HDEVINFO devInfoSet = SetupDiGetClassDevsW(&hidGuid, wid.c_str(), nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (!devInfoSet)
        return "Unknown";

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData{0};
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    if (SetupDiEnumDeviceInterfaces(devInfoSet, nullptr, &hidGuid, 0, &deviceInterfaceData) == FALSE)
        return "Unknown";

    DWORD requiredSize;
    if (SetupDiGetDeviceInterfaceDetailW(
        devInfoSet, &deviceInterfaceData, nullptr, 0, &requiredSize, nullptr
    ) == FALSE && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        return "Unknown";

    std::vector<char> detailData(requiredSize);
    auto detailDataPtr = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailData.data());
    detailDataPtr->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
    if (SetupDiGetDeviceInterfaceDetailW(
        devInfoSet, &deviceInterfaceData, detailDataPtr, requiredSize, nullptr, nullptr
    ) == FALSE)
        return "Unknown";

    wil::unique_hfile devHandle(CreateFileW(
        detailDataPtr->DevicePath,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    ));
    if (devHandle.get() == INVALID_HANDLE_VALUE)
        return "Unknown";

    std::wstring productString(126, '\0');
    if (HidD_GetProductString(devHandle.get(), productString.data(), productString.size()) == FALSE)
        return "Unknown";

    return wstring_to_string(productString);
}
