#include "helper_os.h"
#include <string>

#ifdef _WIN32
    #include <windows.h>
#endif

#ifdef __linux__
    #include <sys/utsname.h>
#endif

const char* get_os_name()
{
#if defined(_WIN32)
    return "Windows";
#elif defined(__linux__)
    return "Linux";
#else
    return "Unknown";
#endif
}

const char* get_arch()
{
#if defined(_M_ARM) || defined(__arm__)
    return "arm";
#elif defined(_M_ARM64) || defined(__aarch64__)
    return "aarch64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#elif defined(_M_X64) || defined(__x86_64__)
    return "x64";
#endif
}

const char* get_os_version()
{
    static std::string ver;
    if (ver.size())
        return ver.c_str();
#if defined(__linux__)
    utsname buffer;
    uname(&buffer);
    ver = buffer.release;
#else
    ver = "Unknown";
#endif
    return ver.c_str();
}

#ifdef _WIN32
bool is_safe_mode()
{
    return (bool)GetSystemMetrics(SM_CLEANBOOT);
}
#endif
