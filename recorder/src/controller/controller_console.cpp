#include "../exporter/exporter.h"
#include "../serializer/serializer.h"
#include "controller.h"
#include <recorder.h>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <print>
#include <thread>
#include <utility>

#if defined(__linux__)
#include <sys/fsuid.h>
#endif

#ifdef _WIN32
#include <conio.h>
#endif

using namespace std::literals;

#ifdef __linux__
// https://gist.github.com/ninovsnino/f910701ea912f03b73a1d0895d213b0e
#include <cstdio>
#include <termios.h>

namespace {

// The terminal state lives in function-local statics instead of namespace
// scope variables (I.2, R.6).
struct termios& termiosOld()
{
    static struct termios state {};
    return state;
}

void init()
{
    struct termios termiosNew {};
    tcgetattr(0, &termiosOld());
    termiosNew = termiosOld();
    termiosNew.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &termiosNew);
}
void reset() { tcsetattr(0, TCSANOW, &termiosOld()); }
int getchLinux()
{
    init();
    int ch = getchar();
    reset();
    return ch;
}

} // namespace

#endif

namespace Kbi {

namespace {

int portableGetch()
{
#if defined(_WIN32)
    return _getch();
#elif defined(__linux__)
    return getchLinux();
#endif
    return -1;
}

} // namespace

ConsoleController::ConsoleController(Recorder& recorder, std::shared_ptr<spdlog::logger> logger)
    : m_ctx { .recorder = recorder, .logger = std::move(logger) }
{
}

void ConsoleController::run()
{
    auto& rec = m_ctx.recorder;
    rec.start();
    std::println("Keep spamming, press Esc to end...");
    std::jthread statThread([&] {
        while (rec.recording()) {
            std::print("\rDevices: {}, Inputs: {}", rec.deviceCount(), rec.inputCount());
            std::fflush(stdout);
            std::this_thread::sleep_for(100ms);
        }
        std::println();
    });
    while (portableGetch() != 27 /* ESC */) {
    }
    rec.stop();
    statThread.join();
    const auto& inputs = rec.inputs();
    std::println("Recorded {} devices", inputs.size());
    inputs.cvisit_all([&](const Recorder::InputMap::value_type& inputPair) {
        const auto& [deviceId, events] = inputPair;
        if (events.empty())
            return;
        std::println("- Device {}", deviceId);
        const auto& deviceMap = rec.devices();
        deviceMap.cvisit(deviceId, [&](const Recorder::DeviceMap::value_type& devicePair) {
            std::println("  - Name: {}", devicePair.second.name);
        });
        std::println("  - Recorded {} events", events.size());
        std::println("  - First 100 diffs:");
        for (std::size_t i = 1; i < std::min<std::size_t>(101, events.size()); i++)
            std::println("    - Diff {}: {}us", i, events[i].timestamp - events[i - 1].timestamp);
    });

#ifdef __linux__
    uid_t uid = getuid();
    if (const char* sudoUid = std::getenv("SUDO_UID")) {
        // Leave the uid untouched if it cannot be parsed.
        std::from_chars(sudoUid, sudoUid + std::strlen(sudoUid), uid);
    }
    setfsuid(uid);
#endif
    // Signal failure of the exports instead of silently producing empty or
    // corrupt files (E.2).
    ExporterMatKbi exporter(rec);
    if (std::fstream fout("out.kbi", std::ios::out | std::ios::binary | std::ios::trunc); fout) {
        exporter.exportTo(fout);
        if (!fout)
            std::println(stderr, "Failed to write out.kbi");
    } else
        std::println(stderr, "Failed to open out.kbi");

    if (std::fstream foutJson("out.json", std::ios::out | std::ios::binary | std::ios::trunc);
        foutJson) {
        JsonTextSerializer ser;
        ser.serialize(rec, foutJson);
        if (!foutJson)
            std::println(stderr, "Failed to write out.json");
    } else
        std::println(stderr, "Failed to open out.json");
#ifdef __linux__
    setfsuid(geteuid());
#endif
}

} // namespace Kbi
