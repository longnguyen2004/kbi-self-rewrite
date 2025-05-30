#include <recorder.h>
#include "exporter/exporter.h"
#include <chrono>
#include <iostream>
#include <fstream>
#include <thread>
#include <cmath>
#include <cstdlib>

#if defined(__linux__)
#include <sys/fsuid.h>
#endif

using namespace std::literals;

extern "C" int portable_getch();

int main(int argc, char const *argv[])
{
    Recorder rec;
    rec.Start();
    std::cout << "Keep spamming, press Esc to end...\n";
    std::thread stat_thread([&rec]()
    {
        while (rec.Recording())
        {
            std::cout << '\r';
            std::cout << "Devices: " << rec.DeviceCount() << ", Inputs: " << rec.InputCount();
            std::this_thread::sleep_for(100ms);
        }
        std::cout << '\n';
    });
    while (portable_getch() != 27 /* ESC */)
    {
    }
    rec.Stop();
    stat_thread.join();
    auto& inputs = rec.Inputs();
    std::cout << "Recorded " << inputs.size() << " devices\n";
    inputs.cvisit_all([&](const Recorder::InputMap::value_type& input_pair) {
        auto& [device_id, events] = input_pair;
        if (events.size() == 0)
            return;
        std::cout << "- Device " << device_id << '\n';
        auto& device_map = rec.Devices();
        device_map.cvisit(device_id, [&](const Recorder::DeviceMap::value_type& device_pair) {
            std::cout << "  - Name: " << device_pair.second.Name << '\n';
        });
        std::cout << "  - Recorded " << events.size() << " events\n";
        std::cout << "  - First 100 diffs:\n";
        for (std::size_t i = 1; i < std::min((std::size_t)101, events.size()); i++)
        {
            std::cout << "    - Diff " << i << ": "
                << events[i].Timestamp - events[i - 1].Timestamp << "us\n";
        }
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
#ifdef __linux__
    setfsuid(geteuid());
#endif
    return 0;
}
