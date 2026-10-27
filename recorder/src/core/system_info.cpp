#include <system_info.h>

#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <Wbemidl.h>
#include <comdef.h>
#include <wil/com.h>
#include <wil/result.h>
#include <windows.h>

#include <exception>
#include <format>
#include <simdutf.h>

namespace Kbi {
namespace {

IWbemServices* connectCim()
{
    static wil::unique_couninitialize_call init;
    static wil::com_ptr_t<IWbemLocator> pLocator;
    static wil::com_ptr_t<IWbemServices> pServices;
    if (pServices)
        return pServices.get();
    if (!init)
        init = wil::CoInitializeEx();
    CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
    if (!pLocator)
        pLocator = wil::CoCreateInstance<IWbemLocator>(CLSID_WbemLocator, CLSCTX_INPROC_SERVER);
    if (!pServices)
        THROW_IF_FAILED(pLocator->ConnectServer(
            bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, nullptr, nullptr, &pServices));
    THROW_IF_FAILED(CoSetProxyBlanket(pServices.get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr));
    return pServices.get();
}

std::string queryCim(std::wstring_view column, std::wstring_view table)
{
    auto cim = connectCim();
    auto query = std::format(L"SELECT {} FROM {}", column, table);
    wil::com_ptr_nothrow<IEnumWbemClassObject> pEnumerator = nullptr;
    THROW_IF_FAILED(cim->ExecQuery(bstr_t(L"WQL"), bstr_t(query.c_str()),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &pEnumerator));

    wil::com_ptr_t<IWbemClassObject> pClassObject = nullptr;
    ULONG uReturn = 0;
    for (;;) {
        if (FAILED(pEnumerator->Next(WBEM_INFINITE, 1, &pClassObject, &uReturn)) || uReturn == 0)
            break;
        VARIANT vtProp;
        VariantInit(&vtProp);
        if (FAILED(pClassObject->Get(column.data(), 0, &vtProp, 0, 0))) {
            VariantClear(&vtProp);
            continue;
        }
        const auto str = vtProp.bstrVal;
        const auto len = SysStringLen(str);
        const auto lenUtf8
            = simdutf::utf8_length_from_utf16(reinterpret_cast<const char16_t*>(str), len);
        std::string value(lenUtf8, 0);
        simdutf::convert_utf16_to_utf8(reinterpret_cast<const char16_t*>(str), len, value.data());
        // The VARIANT owns the BSTR; release it before returning (R.1).
        VariantClear(&vtProp);
        return value;
    }
    return "";
}

const std::string& windowsEdition()
{
    static std::string edition;
    if (edition.size())
        return edition;
    edition = "Unknown";
    try {
        edition = queryCim(L"Caption", L"Win32_OperatingSystem");
    } catch (const std::exception&) {
    }
    return edition;
}

const std::string& cpuName()
{
    static std::string name;
    if (name.size())
        return name;
    name = "Unknown";
    try {
        name = queryCim(L"Name", L"Win32_Processor");
    } catch (const std::exception&) {
    }
    return name;
}

const std::string& architecture()
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

    const auto kernel32 = GetModuleHandleW(L"kernel32");
    BOOL (*IsWow64Process2)(HANDLE hProcess, USHORT* pProcessMachine, USHORT* pNativeMachine);
    *reinterpret_cast<FARPROC*>(&IsWow64Process2) = GetProcAddress(kernel32, "IsWow64Process2");
    [&]() {
        if (IsWow64Process2) {
            USHORT processMachine, nativeMachine;
            if (SUCCEEDED(IsWow64Process2(GetCurrentProcess(), &processMachine, &nativeMachine))) {
                if (processMachine == IMAGE_FILE_MACHINE_UNKNOWN)
                    // Not running under WOW, don't do anything
                    return;
                switch (nativeMachine) {
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

const std::string& windowsVersion()
{
    static std::string osVersion;
    if (osVersion.size())
        return osVersion;

    NTSTATUS(WINAPI * RtlGetVersion)(LPOSVERSIONINFOEXW);
    OSVERSIONINFOEXW osInfo;

    *reinterpret_cast<FARPROC*>(&RtlGetVersion)
        = GetProcAddress(GetModuleHandleA(L"ntdll"), "RtlGetVersion");
    if (RtlGetVersion != nullptr) {
        osInfo.dwOSVersionInfoSize = sizeof(osInfo);
        if (RtlGetVersion(&osInfo) >= 0)
            osVersion = std::format(
                "{}.{}.{}", osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber);
    } else {
        osVersion = "Unknown";
    }
    return osVersion;
}

} // namespace

SystemInfo systemInfo()
{
    LARGE_INTEGER clockFreq;
    QueryPerformanceFrequency(&clockFreq);
    return { .common = { .osName = windowsEdition(),
                 .osVersion = windowsVersion(),
                 .architecture = architecture(),
                 .cpuName = cpuName() },
        .isSafeMode = static_cast<bool>(GetSystemMetrics(SM_CLEANBOOT)),
        .clockFrequency = static_cast<double>(clockFreq.QuadPart) };
}

} // namespace Kbi

#elif defined(__linux__)

#include <sys/utsname.h>

namespace Kbi {
namespace {

std::string readCpuName()
{
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (!cpuinfo)
        return {};
    std::string line;
    while (std::getline(cpuinfo, line)) {
        if (line.starts_with("model name")) {
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

SystemInfo systemInfo()
{
    // Kernel information
    utsname buffer {};
    uname(&buffer);

    // Architecture
#if defined(__i386__)
    const std::string arch = "i386";
#elif defined(__amd64__)
    const std::string arch = "amd64";
#else
    const std::string arch = "Unknown";
#endif

    // Distro information
    std::string distroName;
    std::string distroVer;
    std::ifstream osRelease("/etc/os-release");
    if (osRelease) {
        std::string line;
        while (std::getline(osRelease, line)) {
            if (line.starts_with("NAME=")) {
                distroName = line.substr(5);
                if (distroName.size() >= 2 && distroName.front() == '"' && distroName.back() == '"')
                    distroName = distroName.substr(1, distroName.size() - 2);
            } else if (line.starts_with("VERSION=")) {
                distroVer = line.substr(8);
                if (distroVer.size() >= 2 && distroVer.front() == '"' && distroVer.back() == '"')
                    distroVer = distroVer.substr(1, distroVer.size() - 2);
            }
        }
    }

    // Clock source
    std::ifstream clocksource("/sys/devices/system/clocksource/clocksource0/current_clocksource");
    std::string currentClocksource;
    std::getline(clocksource, currentClocksource);

    constexpr std::int64_t res = -1;
    // (this doesn't work because clock_getres always return 1ns for high res clocks)
    // timespec tp;
    // if (clock_getres(CLOCK_MONOTONIC, &tp) == 0);
    //     res = tp.tv_sec * 1000000000ULL + tp.tv_nsec;
    return { .common = { .osName = (buffer.sysname != nullptr) ? buffer.sysname : "Unknown",
                 .osVersion = (buffer.release != nullptr) ? buffer.release : "Unknown",
                 .architecture = arch,
                 .cpuName = readCpuName() },
        .distroName = distroName,
        .distroVersion = distroVer,
        .clockSource = currentClocksource,
        .clockFrequency = res == -1 ? 0 : 1e9 / res };
}

} // namespace Kbi

#endif
