#include "info.h"
#include <comdef.h>
#include <windows.h>
#include <Wbemidl.h>
#include <wil/com.h>
#include <wil/result.h>

#include <exception>
#include <format>
#include <string>
#include <simdutf.h>

IWbemServices* connect_cim()
{
    static wil::unique_couninitialize_call init;
    static wil::com_ptr_t<IWbemLocator> pLocator;
    static wil::com_ptr_t<IWbemServices> pServices;
    if (pServices)
        return pServices.get();
    if (!init)
        init = wil::CoInitializeEx();
    CoInitializeSecurity(
        nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr
    );
    if (!pLocator)
        pLocator = wil::CoCreateInstance<IWbemLocator>(CLSID_WbemLocator, CLSCTX_INPROC_SERVER);
    if (!pServices)
        THROW_IF_FAILED(pLocator->ConnectServer(
            bstr_t("ROOT\\CIMV2"), nullptr, nullptr, 0, 0, nullptr, nullptr, &pServices
        ));
    THROW_IF_FAILED(CoSetProxyBlanket(
        pServices.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
        nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE
    ));
    return pServices.get();
}

std::string query_cim(std::wstring_view column, std::wstring_view table)
{
    auto cim = connect_cim();
    auto query = std::format(L"SELECT {} FROM {}", column, table);
    wil::com_ptr_nothrow<IEnumWbemClassObject> pEnumerator = nullptr;
    THROW_IF_FAILED(cim->ExecQuery(
        bstr_t("WQL"),
        bstr_t(query.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &pEnumerator
    ));

    wil::com_ptr_t<IWbemClassObject> pClassObject = nullptr;
    ULONG uReturn = 0;
    while (pEnumerator->Next(WBEM_INFINITE, 1, &pClassObject, &uReturn), uReturn != 0)
    {
        VARIANT vtProp;
        if (FAILED(pClassObject->Get(column.data(), 0, &vtProp, 0, 0)))
            continue;
        auto str = vtProp.bstrVal;
        auto len = SysStringLen(str);
        auto len_utf8 = simdutf::utf8_length_from_utf16(
            reinterpret_cast<const char16_t*>(str), len
        );
        std::string value(len_utf8, 0);
        simdutf::convert_utf16_to_utf8(
            reinterpret_cast<const char16_t*>(str), len,
            value.data()
        );
        return value;
    }
    return "";
}

const std::string& get_windows_edition()
{
    static std::string edition;
    if (edition.size())
        return edition;
    edition = "Unknown";
    try
    {
        edition = query_cim(L"Caption", L"Win32_OperatingSystem");
    }
    catch(std::exception)
    {
    }
    return edition;
}

const std::string& get_cpu_name()
{
    static std::string edition;
    if (edition.size())
        return edition;
    edition = "Unknown";
    try
    {
        edition = query_cim(L"Name", L"Win32_Processor");
    }
    catch(std::exception)
    {
    }
    return edition;
}

const std::string& get_arch()
{
    static std::string arch;
    if (arch.size())
        return arch;
#if defined(_M_IX86)
    arch = "i386";
#elif defined(_M_AMD64)
    arch = "amd64";
#elif defined(_M_ARM)
    arch = "armv7";
#elif defined(_M_ARM64)
    arch = "aarch64";
#endif

    auto kernel32 = GetModuleHandleW(L"kernel32");
    BOOL(*IsWow64Process2)(HANDLE hProcess, USHORT* pProcessMachine, USHORT* pNativeMachine);
    *reinterpret_cast<FARPROC*>(&IsWow64Process2) = GetProcAddress(kernel32, "IsWow64Process2");
    [&]() {
        if (IsWow64Process2)
        {
            USHORT processMachine, nativeMachine;
            if (SUCCEEDED(IsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine)))
            {
                if (processMachine == IMAGE_FILE_MACHINE_UNKNOWN)
                    // Not running under WOW, don't do anything
                    return;
                switch (nativeMachine)
                {
                    case IMAGE_FILE_MACHINE_AMD64:
                        arch += " (running under amd64)";
                        break;
                    case IMAGE_FILE_MACHINE_ARM:
                    case IMAGE_FILE_MACHINE_THUMB:
                    case IMAGE_FILE_MACHINE_ARMNT:
                        arch += " (running under armv7)";
                        break;
                    case IMAGE_FILE_MACHINE_ARM64:
                        arch += " (running under aarch64)";
                        break;
                }
                return;
            }
        }
        BOOL isWow64;
        if (SUCCEEDED(IsWow64Process(GetCurrentProcess(), &isWow64)))
            arch += " (running under WOW64)";
    }();
    return arch;
}

const std::string& get_windows_version()
{
    static std::string os_version;
    if (os_version.size())
        return os_version;

    NTSTATUS(WINAPI *RtlGetVersion)(LPOSVERSIONINFOEXW);
    OSVERSIONINFOEXW osInfo;

    *(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleA("ntdll"), "RtlGetVersion");
    if (RtlGetVersion != nullptr)
    {
        osInfo.dwOSVersionInfoSize = sizeof(osInfo);
        if (RtlGetVersion(&osInfo) >= 0)
            os_version = std::format(
                "{}.{}.{}", osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber
            );
    }
    else
    {
        os_version = "Unknown";
    }
    return os_version;
}

SystemInfo GetSystemInfo()
{
    LARGE_INTEGER clockFreq;
    QueryPerformanceFrequency(&clockFreq);
    return {
        .Common = {
            .OsName = get_windows_edition(),
            .OsVersion = get_windows_version(),
            .Architecture = get_arch(),
            .CpuName = get_cpu_name()
        },
        .IsSafeMode = (bool)GetSystemMetrics(SM_CLEANBOOT),
        .ClockFrequency = static_cast<double>(clockFreq.QuadPart)
    };
}
