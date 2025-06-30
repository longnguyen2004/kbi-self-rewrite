#include "info.h"
#include <sys/utsname.h>
#include <sys/time.h>
#include <fstream>

SystemInfo GetSystemInfo()
{
    // Kernel information
    utsname buffer;
    uname(&buffer);

    // Architecture
    #if defined(__i386__)
    const char* arch = "i386";
    #elif defined(__amd64__)
    const char* arch = "amd64";
    #else
    const char* arch = "Unknown";
    #endif

    // CPU name
    std::string cpu_name;
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo)
    {
        std::string line;
        while (std::getline(cpuinfo, line))
        {
            if (line.starts_with("model name"))
            {
                cpu_name = line.substr(line.find(':') + 2);
                break;
            }
        }
    }

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

    // Clock frequency
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
            .CpuName = cpu_name
        },
        .DistroName = distro_name,
        .DistroVersion = distro_ver,
        .ClockSource = current_clocksource,
        .ClockFrequency = res == -1 ? 0 : 1e9 / res
    };
}
