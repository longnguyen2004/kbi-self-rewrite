#pragma once

#include <cstdint>
#include <string>

// The information about the machine the recorder runs on.
struct SystemInfoCommon {
    std::string OsName;
    std::string OsVersion;
    std::string Architecture;
    std::string CpuName;
};

#if defined(_WIN32)
struct SystemInfoWindows {
    SystemInfoCommon Common;
    bool IsSafeMode = false;
    double ClockFrequency = 0.0;
};
using SystemInfo = SystemInfoWindows;
#elif defined(__linux__)
struct SystemInfoLinux {
    SystemInfoCommon Common;
    std::string DistroName;
    std::string DistroVersion;
    std::string ClockSource;
    double ClockFrequency = 0.0;
};
using SystemInfo = SystemInfoLinux;
#else
struct SystemInfoUnknown {
    SystemInfoCommon Common;
};
using SystemInfo = SystemInfoUnknown;
#endif

// Collects the information about the machine.
[[nodiscard]] SystemInfo GetSystemInfo();

// Whether the machine was started in safe mode, it is only known on Windows.
[[nodiscard]] inline bool is_safe_mode(const SystemInfo& info) noexcept {
#if defined(_WIN32)
    return info.IsSafeMode;
#else
    (void)info;
    return false;
#endif
}
