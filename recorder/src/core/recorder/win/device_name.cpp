#include "device_name.h"
#include <wil/resource.h>
#include <wil/result.h>
#include <windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <initguid.h>
#include <devguid.h>
#include <devpkey.h>
#include <usbiodef.h>
#include <usbioctl.h>
#include <wdmguid.h>

#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <charconv>
#include <optional>
#include <vector>

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

std::string device_name_from_pnp(std::wstring_view wid)
{
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfoSet = SetupDiGetClassDevsW(&hidGuid, wid.data(), nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
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

    std::vector<unsigned char> detailData(requiredSize);
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

std::string device_name_from_pnp(std::string_view id)
{
    return device_name_from_pnp(string_to_wstring(id));
}

std::optional<std::vector<unsigned char>> get_usb_device_info(std::wstring_view wid)
{
    DEVINST current, parent;
    // Get device instance handle for the HID device
    if (CM_Locate_DevNodeW(&current, const_cast<wchar_t*>(wid.data()), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS)
        return {};

    // Get parent of the HID device
    if (CM_Get_Parent(&parent, current, 0) != CR_SUCCESS)
        return {};
    current = parent;

    // Check if the device is a USB device
    DEVPROPTYPE propType;
    GUID busTypeGuid;
    ULONG size = sizeof(busTypeGuid);
    if (CM_Get_DevNode_PropertyW(
        current, &DEVPKEY_Device_BusTypeGuid, &propType, reinterpret_cast<PBYTE>(&busTypeGuid), &size, 0
    ) != CR_SUCCESS || propType != DEVPROP_TYPE_GUID)
        return {};
    if (!IsEqualGUID(busTypeGuid, GUID_BUS_TYPE_USB))
        return {};

    // This is a USB device, walk up the tree to find the hub
    while (true)
    {
        if (CM_Get_Parent(&parent, current, 0) != CR_SUCCESS)
            return {};
        DEVPROPTYPE propType;
        ULONG size = 0;
        CONFIGRET cret;
        if (CM_Get_DevNode_PropertyW(parent, &DEVPKEY_Device_Service, &propType, nullptr, &size, 0) != CR_BUFFER_SMALL)
            return {};
        std::wstring service(size / 2, 0);
        if (CM_Get_DevNode_PropertyW(
            parent, &DEVPKEY_Device_Service, &propType, reinterpret_cast<PBYTE>(service.data()), &size, 0
        ) != CR_SUCCESS || propType != DEVPROP_TYPE_STRING)
            return {};
        if (boost::ifind_first(service, L"usbhub"))
            // Found the hub, parent will be the hub, current will the be (composite) device
            break;
        current = parent;

    }

    // Get the port info of the device
    size = 0;
    if (CM_Get_DevNode_PropertyW(current, &DEVPKEY_Device_LocationInfo, &propType, nullptr, &size, 0) != CR_BUFFER_SMALL)
        return {};
    std::wstring locationInfo(size / 2, 0);
    if (CM_Get_DevNode_PropertyW(
        current, &DEVPKEY_Device_LocationInfo, &propType, reinterpret_cast<PBYTE>(locationInfo.data()), &size, 0
    ) != CR_SUCCESS || propType != DEVPROP_TYPE_STRING)
        return {};
    locationInfo = locationInfo.substr(
        locationInfo.find(L"Port_#") + 6, 4
    );
    int port = _wtoi(locationInfo.c_str());
    if (!port)
        return {};
    
    // Get the hub device ID
    ULONG deviceIdSize = 0;
    if (CM_Get_Device_ID_Size(&deviceIdSize, parent, 0) != CR_SUCCESS)
        return {};
    std::wstring hubDeviceId(deviceIdSize, 0);
    if (CM_Get_Device_IDW(parent, hubDeviceId.data(), deviceIdSize, 0) != CR_SUCCESS)
        return {};
    for (auto& c: hubDeviceId)
    {
        if (c == L'\\')
            c = L'#';
    }

    // Open a handle to the USB hub
    auto devicePath = std::format(LR"(\\?\{}#{{f18a0e88-c30c-11d0-8815-00a0c906bed8}})", hubDeviceId);
    wil::unique_hfile hHubDevice(CreateFileW(
        devicePath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    ));
    if (hHubDevice.get() == INVALID_HANDLE_VALUE)
        return {};

    std::vector<char> buffer;
    ULONG bytesReturned;
    // Get device descriptor and device speed
    ULONG confInfoExSize = sizeof(USB_NODE_CONNECTION_INFORMATION_EX) + sizeof(USB_PIPE_INFO) * 30;
    buffer.resize(confInfoExSize);
    std::fill(buffer.begin(), buffer.end(), 0);
    auto connectionInfoEx = reinterpret_cast<PUSB_NODE_CONNECTION_INFORMATION_EX>(buffer.data());
    connectionInfoEx->ConnectionIndex = port;
    auto success = DeviceIoControl(hHubDevice.get(),
        IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX,
        connectionInfoEx,
        confInfoExSize,
        connectionInfoEx,
        confInfoExSize,
        &bytesReturned,
        nullptr
    );
    if (success == FALSE)
        return {};
    auto speed = connectionInfoEx->Speed;
    auto deviceDescriptor = connectionInfoEx->DeviceDescriptor;
    auto deviceDescriptorPtr = reinterpret_cast<unsigned char*>(&deviceDescriptor);
    std::vector<unsigned char> deviceDescriptorBytes(
        deviceDescriptorPtr,
        deviceDescriptorPtr + deviceDescriptor.bLength
    );

    // Get configuration descriptor
    ULONG confDescReqSize = sizeof(USB_DESCRIPTOR_REQUEST) + sizeof(USB_CONFIGURATION_DESCRIPTOR);
    buffer.resize(confDescReqSize);
    std::fill(buffer.begin(), buffer.end(), 0);
    auto confDescReq = reinterpret_cast<PUSB_DESCRIPTOR_REQUEST>(buffer.data());
    auto confDesc = reinterpret_cast<PUSB_CONFIGURATION_DESCRIPTOR>(confDescReq->Data);
    confDescReq->ConnectionIndex = port;
    confDescReq->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8);
    confDescReq->SetupPacket.wLength = sizeof(USB_CONFIGURATION_DESCRIPTOR);
    confDescReq->SetupPacket.bmRequest = 0x80;
    confDescReq->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
    success = DeviceIoControl(hHubDevice.get(),
        IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
        confDescReq,
        confDescReqSize,
        confDescReq,
        confDescReqSize,
        &bytesReturned,
        nullptr
    );
    if (success == FALSE)
        return {};

    // Get the entire configuration descriptor
    confDescReqSize = sizeof(USB_DESCRIPTOR_REQUEST) + confDesc->wTotalLength;
    buffer.resize(confDescReqSize);
    std::fill(buffer.begin(), buffer.end(), 0);
    confDescReq = reinterpret_cast<PUSB_DESCRIPTOR_REQUEST>(buffer.data());
    confDesc = reinterpret_cast<PUSB_CONFIGURATION_DESCRIPTOR>(confDescReq->Data);
    confDescReq->ConnectionIndex = port;
    confDescReq->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8);
    confDescReq->SetupPacket.wLength = confDesc->wTotalLength;
    confDescReq->SetupPacket.bmRequest = 0x80;
    confDescReq->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
    success = DeviceIoControl(hHubDevice.get(),
        IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION,
        confDescReq,
        confDescReqSize,
        confDescReq,
        confDescReqSize,
        &bytesReturned,
        nullptr
    );
    if (success == FALSE)
        return {};
    
    std::vector<unsigned char> configurationDescriptorBytes(
        reinterpret_cast<unsigned char*>(confDesc),
        reinterpret_cast<unsigned char*>(confDesc) + confDesc->wTotalLength
    );
    return {};
}

void device_info_from_pnp(std::string_view id)
{
    auto wid = string_to_wstring(id);
    std::string name;
    if (id.starts_with("HID"))
        name = device_name_from_pnp(wid);
    get_usb_device_info(wid);
}
