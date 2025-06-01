#include "exporter.h"
#include <keycode.h>
#include <array>
#include <boost/endian.hpp>
#include <chrono>
#include <functional>
#include <iterator>
#include <list>
#include <numeric>
#include <stdexcept>
#include <string>
#include <unordered_map>

using namespace boost::endian;
using namespace std::chrono;

static const std::array<char, 4> KBI_HEADER = {'K', 'B', 'I', 0};
static const std::string CREATOR = "kbi_cpp_recorder";

struct KbiInput
{
    std::string Key;
    std::int64_t Source;
};

namespace std {
    template <>
    struct hash<KbiInput>
    {
        std::size_t operator()(const KbiInput& input) const
        {
            return std::hash<std::string>()(input.Key) ^ std::hash<std::int64_t>()(input.Source);
        }
    };
}

bool operator==(const KbiInput& a, const KbiInput& b)
{
    return a.Key == b.Key && a.Source == b.Source;
}

struct KbiEvent
{
    double Time;
    bool Pressed;
    KbiInput Input;
};

struct KbiInputInfo
{
    std::int32_t Color;
    bool Visible;
};

void write_uleb128(std::ostream& out, std::uint64_t x)
{
    do {
        char byte = x & 0x7F;
        x >>= 7;
        if (x != 0)
            byte |= 0x80;
        out.put(byte);
    }
    while (x != 0);
}

void write_bool(std::ostream& out, bool x)
{
    out.put(x ? 1 : 0);
}

void write_int32(std::ostream& out, std::int32_t x)
{
    std::array<char, 4> buffer;
    store_little_s32(reinterpret_cast<unsigned char*>(buffer.data()), x);
    out.write(buffer.data(), buffer.size());
}

void write_uint32(std::ostream& out, std::uint32_t x)
{
    std::array<char, 4> buffer;
    store_little_u32(reinterpret_cast<unsigned char*>(buffer.data()), x);
    out.write(buffer.data(), buffer.size());
}

void write_int64(std::ostream& out, std::int64_t x)
{
    std::array<char, 8> buffer;
    store_little_s64(reinterpret_cast<unsigned char*>(buffer.data()), x);
    out.write(buffer.data(), buffer.size());
}

void write_uint64(std::ostream& out, std::uint64_t x)
{
    std::array<char, 8> buffer;
    store_little_u64(reinterpret_cast<unsigned char*>(buffer.data()), x);
    out.write(buffer.data(), buffer.size());
}

void write_double(std::ostream& out, double x)
{
    auto x_bits = std::bit_cast<std::uint64_t>(x);
    write_uint64(out, x_bits);
}

void write_string(std::ostream& out, std::string_view str)
{
    write_uleb128(out, str.size());
    out.write(str.data(), str.size());
}

template <typename Iterator> requires std::forward_iterator<Iterator>
void write_list(
    std::ostream& out, Iterator start, Iterator end,
    std::function<void(std::ostream&, const typename std::iterator_traits<Iterator>::value_type&)> serializer
)
{
    auto count = std::distance(start, end);
    write_int32(out, count);
    for (auto it = start; it != end; it++)
        serializer(out, *it);
}

void write_time(std::ostream& out, time_point<system_clock, nanoseconds> t)
{
    // DateTime binary format
    // Bits 00 - 61: The number of 100-nanosecond ticks since midnight 1/1/0001
    // Bits 62 - 63: DateTimeKind (0 = Unspecified, 1 = Utc, 2 = Local)

    auto t_adjusted = t + (sys_days(January/1/1970) - sys_days(January/1/0001));
    std::uint64_t ticks = t_adjusted.time_since_epoch().count() / 100;
    ticks = ticks << 2 & 1; // Utc
    write_uint64(out, ticks);
}

void write_kbiinput(std::ostream& out, const KbiInput& input)
{
    write_string(out, input.Key);
    write_int64(out, input.Source);
}

void Exporter_MatKbi::Export(std::ostream& out)
{
    using DeviceMap = Recorder::DeviceMap;
    using InputMap = Recorder::InputMap;
    // Write header
    out.write(KBI_HEADER.data(), KBI_HEADER.size());

    // Write file version. Hardcode as 3 for now
    write_uint32(out, 3);

    // Write creator
    switch (m_recorder.Backend())
    {
        case RecorderBackend::WINDOWS_GAMEINPUT:
            write_string(out, CREATOR + " (Backend: Windows GameInput)");
            break;

        case RecorderBackend::LINUX_EVDEV:
            write_string(out, CREATOR + " (Backend: Linux evdev)");
            break;

        default:
            write_string(out, CREATOR + " (Backend: Unknown)");
            break;
    }

    // Write title
    write_string(out, "Testing testing");

    // Write recorded time
    write_time(out, m_recorder.StartTime());

    // Write elapsed time
    write_double(out, static_cast<duration<double>>(m_recorder.Elapsed()).count());

    auto& devices = m_recorder.Devices();
    auto& inputs = m_recorder.Inputs();

    // Build a map to map IDs to an arbitrary index
    // KBI uses indices to associate events with their corresponding devices
    std::unordered_map<std::string, std::int64_t> index_map;
    std::int64_t i = 0;
    devices.cvisit_all([&](const DeviceMap::value_type& device) {
        index_map[device.first] = i++;
    });

    // Create events and input info lists
    std::list<KbiEvent> kbi_events;
    std::unordered_map<KbiInput, KbiInputInfo> kbi_input_info;

    inputs.visit_all([&](const InputMap::value_type& device) {
        auto& [id, events] = device;
        for (auto& event: events)
        {
            KbiInput input{std::string{keycode_to_string(event.Code)}, index_map[id]};
            kbi_events.emplace_back(
                event.Timestamp / 1000000.0,
                event.Pressed,
                input
            );
            kbi_input_info.try_emplace(
                input,
                0xFFA9A9A9, true // Default color and visibility
            );
        }
    });
    write_list(
        out, kbi_events.begin(), kbi_events.end(),
        [](std::ostream& out, const KbiEvent& event) {
            write_double(out, event.Time);
            write_bool(out, event.Pressed);
            write_kbiinput(out, event.Input);
        }
    );

    // Write sources
    std::list<DeviceMap::value_type> devices_list;
    devices.cvisit_all([&](const DeviceMap::value_type& device) {
        devices_list.push_back(device);
    });
    write_list(
        out, devices_list.begin(), devices_list.end(),
        [&](std::ostream& out, const std::pair<std::string, Device>& device) {
            write_int64(out, index_map[device.first]);
            inputs.cvisit(device.first, [&](const InputMap::value_type& input) {
                write_int32(out, input.second.size());
            });
            write_string(out, device.second.Name);
            write_string(out, device.first);
        }
    );

    // Write analysis parameters
    write_int32(out, 16000); // Bin rate, default to 16000 for now
    write_int32(out, 0); // HPS elimination
    write_bool(out, true); // Low cut

    // Write input info
    write_list(
        out, kbi_input_info.begin(), kbi_input_info.end(),
        [](std::ostream& out, const std::pair<KbiInput, KbiInputInfo>& pair) {
            const auto& [input, info] = pair;
            write_kbiinput(out, input);
            write_int32(out, info.Color);
            write_bool(out, info.Visible);
        }
    );
}