#include <system_info.h>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <comdef.h>
#include <windows.h>
#include <Wbemidl.h>
#include <wil/com.h>
#include <wil/result.h>

#include <exception>
#include <format>
#include <simdutf.h>

namespace {

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
            bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, nullptr, nullptr, &pServices
        ));
    THROW_IF_FAILED(CoSetProxyBlanket(
        pServices.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE,
        nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr
    ));
    return pServices.get();
}

std::string query_cim(std::wstring_view column, std::wstring_view table)
{
    auto cim = connect_cim();
    auto query = std::format(L"SELECT {} FROM {}", column, table);
    wil::com_ptr_nothrow<IEnumWbemClassObject> pEnumerator = nullptr;
    THROW_IF_FAILED(cim->ExecQuery(
        bstr_t(L"WQL"),
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
    static std::string name;
    if (name.size())
        return name;
    name = "Unknown";
    try
    {
        name = query_cim(L"Name", L"Win32_Processor");
    }
    catch(std::exception)
    {
    }
    return name;
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

    *(FARPROC*)&RtlGetVersion = GetProcAddress(GetModuleHandleA(L"ntdll"), "RtlGetVersion");
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

} // namespace

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

#elif defined(__linux__)

#include <sys/utsname.h>

namespace {

std::string read_cpu_name()
{
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo)
        return {};
    std::string line;
    while (std::getline(cpuinfo, line))
    {
        if (line.rfind("model name", 0) == 0)
        {
            const auto colon = line.find(':');
            if (colon == std::string::npos)
                return {};
            std::string value = line.substr(colon + 1);
            const auto first = value.find_first_not_of(" \t");
            return first == std::string::npos ? std::string() : value.substr(first);
        }
    }
    return {};
}

} // namespace

SystemInfo GetSystemInfo()
{
    // Kernel information
    utsname buffer{};
    uname(&buffer);

    // Architecture
#if defined(__i386__)
    const char* arch = "i386";
#elif defined(__amd64__)
    const char* arch = "amd64";
#else
    const char* arch = "Unknown";
#endif

    // Distro information
    std::string distro_name, distro_ver;
    std::ifstream os_release("/etc/os-release");
    if (os_release)
    {
        std::string line;
        while (std::getline(os_release, line))
        {
            if (line.starts_with("NAME="))
            {
                distro_name = line.substr(5);
                if (distro_name[0] == '"')
                    distro_name = distro_name.substr(1, distro_name.size() - 2);
            }
            else if (line.starts_with("VERSION="))
            {
                distro_ver = line.substr(8);
                if (distro_ver[0] == '"')
                    distro_ver = distro_name.substr(1, distro_ver.size() - 2);
            }
        }
    }

    // Clock source
    std::ifstream clocksource("/sys/devices/system/clocksource/clocksource0/current_clocksource");
    std::string current_clocksource;
    std::getline(clocksource, current_clocksource);

    std::int64_t res = -1;
    // (this doesn't work because clock_getres always return 1ns for high res clocks)
    // timespec tp;
    // if (clock_getres(CLOCK_MONOTONIC, &tp) == 0);
    //     res = tp.tv_sec * 1000000000ULL + tp.tv_nsec;
    return {
        .Common = {
            .OsName = buffer.sysname ? buffer.sysname : "Unknown",
            .OsVersion = buffer.release ? buffer.release : "Unknown",
            .Architecture = arch,
            .CpuName = read_cpu_name()
        },
        .DistroName = distro_name,
        .DistroVersion = distro_ver,
        .ClockSource = current_clocksource,
        .ClockFrequency = res == -1 ? 0 : 1e9 / res
    };
}

#endif
