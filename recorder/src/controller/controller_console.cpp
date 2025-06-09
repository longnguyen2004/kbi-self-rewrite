#include "controller.h"
#include "../exporter/exporter.h"
#include "../serializer/serializer.h"
#include <recorder.h>
#include <chrono>
#include <print>
#include <fstream>
#include <thread>
#include <cmath>
#include <cstdlib>

using namespace std::literals;

#if defined(__linux__)
#include <sys/fsuid.h>
#endif

#ifdef _WIN32
    #include <conio.h>
#endif

#ifdef __linux__
    // https://gist.github.com/ninovsnino/f910701ea912f03b73a1d0895d213b0e
    #include <termios.h>
    #include <stdio.h>

    static struct termios termios_old, termios_new;
    void init()
    {
        tcgetattr(0, &termios_old);
        termios_new = termios_old;
        termios_new.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(0, TCSANOW, &termios_new);
    }
    void reset()
    {
        tcsetattr(0, TCSANOW, &termios_old);
    }
    int getch_linux()
    {
        init();
        int ch = getchar();
        reset();
        return ch;
    }
#endif

int portable_getch()
{
    #if defined(_WIN32)
        return _getch();
    #elif defined(__linux__)
        return getch_linux();
    #endif
    return -1;
}

ConsoleController::ConsoleController(Recorder& recorder): Controller(recorder)
{
}

void ConsoleController::Run()
{
    auto& rec = m_recorder;
    rec.Start();
    std::println("Keep spamming, press Esc to end...");
    std::thread stat_thread([&]()
    {
        while (rec.Recording())
        {
            std::print("\rDevices: {}, Inputs: {}", rec.DeviceCount(), rec.InputCount());
            std::this_thread::sleep_for(100ms);
        }
        std::println();
    });
    while (portable_getch() != 27 /* ESC */)
    {
    }
    rec.Stop();
    stat_thread.join();
    auto& inputs = rec.Inputs();
    std::println("Recorded {} devices", inputs.size());
    inputs.cvisit_all([&](const Recorder::InputMap::value_type& input_pair) {
        auto& [device_id, events] = input_pair;
        if (events.size() == 0)
            return;
        std::println("- Device {}", device_id);
        auto& device_map = rec.Devices();
        device_map.cvisit(device_id, [&](const Recorder::DeviceMap::value_type& device_pair) {
            std::println("  - Name: {}", device_pair.second.Name);
        });
        std::println("  - Recorded {} events", events.size());
        std::println("  - First 100 diffs:");
        for (std::size_t i = 1; i < std::min((std::size_t)101, events.size()); i++)
            std::println("    - Diff {}: {}us", i, events[i].Timestamp - events[i - 1].Timestamp);
    });

#ifdef __linux__
    if (const char* sudo_uid = std::getenv("SUDO_UID"))
    {
        setfsuid(std::atoi(sudo_uid));
    }
    else
    {
        setfsuid(getuid());
    }
#endif
    std::fstream fout("out.kbi", std::ios::out | std::ios::binary | std::ios::trunc);
    Exporter_MatKbi exporter(rec);
    exporter.Export(fout);

    std::fstream fout_json("out.json", std::ios::out | std::ios::binary | std::ios::trunc);
    JsonTextSerializer ser;
    ser.Serialize(rec, fout_json);
#ifdef __linux__
    setfsuid(geteuid());
#endif
}