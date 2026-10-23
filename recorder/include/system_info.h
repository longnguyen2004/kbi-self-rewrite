#pragma once

#include <cstdint>
#include <string>

namespace Kbi {

// The information about the machine the recorder runs on.
struct SystemInfoCommon {
    std::string osName;
    std::string osVersion;
    std::string architecture;
    std::string cpuName;
};

#if defined(_WIN32)
struct SystemInfoWindows {
    SystemInfoCommon common;
    bool isSafeMode = false;
    double clockFrequency = 0.0;
};
using SystemInfo = SystemInfoWindows;
#elif defined(__linux__)
struct SystemInfoLinux {
    SystemInfoCommon common;
    std::string distroName;
    std::string distroVersion;
    std::string clockSource;
    double clockFrequency = 0.0;
};
using SystemInfo = SystemInfoLinux;
#else
struct SystemInfoUnknown {
    SystemInfoCommon common;
};
using SystemInfo = SystemInfoUnknown;
#endif

// Collects the information about the machine.
[[nodiscard]] SystemInfo systemInfo();

// Whether the machine was started in safe mode, it is only known on Windows.
[[nodiscard]] inline bool isSafeMode(const SystemInfo& info) noexcept
{
#if defined(_WIN32)
    return info.isSafeMode;
#else
    (void)info;
    return false;
#endif
}

} // namespace Kbi
