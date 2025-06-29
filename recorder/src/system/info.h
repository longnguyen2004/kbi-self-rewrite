#include <cstdint>
#include <string>
#include <optional>

struct SystemInfoCommon {
    std::string OsName;
    std::string OsVersion;
    std::string Architecture;
    std::string CpuName;
};

#if defined(_WIN32)
    struct SystemInfoWindows {
        SystemInfoCommon Common;
        bool IsSafeMode;
        double ClockFrequency;
    };
    using SystemInfo = SystemInfoWindows;
#elif defined(__linux__)
    struct SystemInfoLinux {
        SystemInfoCommon Common;
        std::string DistroName;
        std::string DistroVersion;
        std::string ClockSource;
        double ClockFrequency;
    };
    using SystemInfo = SystemInfoLinux;
#endif

SystemInfo GetSystemInfo();
