#include "exporter.h"
#include <datetime.h>
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
#include <string_view>
#include <unordered_map>

namespace Kbi {

using namespace boost::endian;
using namespace std::chrono;

// A single .kbi input, its key name and the index of its device.
struct KbiInput {
    std::string key;
    std::int64_t source;
};

struct KbiEvent {
    double time;
    bool pressed;
    KbiInput input;
};

struct KbiInputInfo {
    std::int32_t color;
    bool visible;
};

namespace {

const std::array<char, 4> kbiHeader = { 'K', 'B', 'I', 0 };
constexpr std::string_view creator = "kbi_cpp_recorder";

} // namespace

} // namespace Kbi

template<> struct std::hash<Kbi::KbiInput> {
    std::size_t operator()(const Kbi::KbiInput& input) const
    {
        return std::hash<std::string>()(input.key) ^ std::hash<std::int64_t>()(input.source);
    }
};

namespace Kbi {

bool operator==(const KbiInput& a, const KbiInput& b)
{
    return a.key == b.key && a.source == b.source;
}

void writeUleb128(std::ostream& out, std::uint64_t x)
{
    do {
        char byte = x & 0x7F;
        x >>= 7;
        if (x != 0)
            byte |= 0x80;
        out.put(byte);
    } while (x != 0);
}

void writeBool(std::ostream& out, bool x) { out.put(x ? 1 : 0); }

void writeInt32(std::ostream& out, std::int32_t x)
{
    std::array<char, 4> buffer;
    store_little_s32(reinterpret_cast<unsigned char*>(buffer.data()), x);
    out.write(buffer.data(), buffer.size());
}

void writeUint32(std::ostream& out, std::uint32_t x)
{
    std::array<char, 4> buffer;
    store_little_u32(reinterpret_cast<unsigned char*>(buffer.data()), x);
    out.write(buffer.data(), buffer.size());
}

void writeInt64(std::ostream& out, std::int64_t x)
{
    std::array<char, 8> buffer;
    store_little_s64(reinterpret_cast<unsigned char*>(buffer.data()), x);
    out.write(buffer.data(), buffer.size());
}

void writeUint64(std::ostream& out, std::uint64_t x)
{
    std::array<char, 8> buffer;
    store_little_u64(reinterpret_cast<unsigned char*>(buffer.data()), x);
    out.write(buffer.data(), buffer.size());
}

void writeDouble(std::ostream& out, double x)
{
    auto xBits = std::bit_cast<std::uint64_t>(x);
    writeUint64(out, xBits);
}

void writeString(std::ostream& out, std::string_view str)
{
    writeUleb128(out, str.size());
    out.write(str.data(), str.size());
}

template<typename Iterator>
    requires std::forward_iterator<Iterator>
void writeList(std::ostream& out, Iterator start, Iterator end,
    const std::function<void(
        std::ostream&, const typename std::iterator_traits<Iterator>::value_type&)>& serializer)
{
    auto count = std::distance(start, end);
    writeInt32(out, count);
    for (auto it = start; it != end; it++)
        serializer(out, *it);
}

void writeTime(std::ostream& out, time_point<system_clock, nanoseconds> t)
{
    // The .NET DateTime binary format, see datetime.h.
    writeUint64(out, toNetDatetimeTicks(t, kNetDateTimeKindUtc));
}

void writeKbiInput(std::ostream& out, const KbiInput& input)
{
    writeString(out, input.key);
    writeInt64(out, input.source);
}

void ExporterMatKbi::exportTo(std::ostream& out)
{
    using DeviceMap = Recorder::DeviceMap;
    using InputMap = Recorder::InputMap;
    // Write header
    out.write(kbiHeader.data(), kbiHeader.size());

    // Write file version. Hardcode as 3 for now
    writeUint32(out, 3);

    // Write creator
    switch (m_recorder.backend()) {
    case RecorderBackend::WindowsGameInput:
        writeString(out, std::string(creator) + " (Backend: Windows GameInput)");
        break;

    case RecorderBackend::LinuxEvdev:
        writeString(out, std::string(creator) + " (Backend: Linux evdev)");
        break;

    default:
        writeString(out, std::string(creator) + " (Backend: Unknown)");
        break;
    }

    // Write title
    writeString(out, "Testing testing");

    // Write recorded time
    writeTime(out, m_recorder.startTime());

    // Write elapsed time
    writeDouble(out, static_cast<duration<double>>(m_recorder.elapsed()).count());

    const auto& devices = m_recorder.devices();
    const auto& inputs = m_recorder.inputs();

    // Build a map to map IDs to an arbitrary index
    // KBI uses indices to associate events with their corresponding devices
    std::unordered_map<std::string, std::int64_t> indexMap;
    std::int64_t i = 0;
    devices.cvisit_all([&](const DeviceMap::value_type& device) { indexMap[device.first] = i++; });

    // Create events and input info lists
    std::list<KbiEvent> kbiEvents;
    std::unordered_map<KbiInput, KbiInputInfo> kbiInputInfo;

    inputs.visit_all([&](const InputMap::value_type& device) {
        const auto& [id, events] = device;
        for (const auto& event : events) {
            KbiInput input { .key = std::string { keycodeToString(event.code) },
                .source = indexMap[id] };
            kbiEvents.emplace_back(event.timestamp / 1000000.0, event.pressed, input);
            kbiInputInfo.try_emplace(input, 0xFFA9A9A9, true // Default color and visibility
            );
        }
    });
    writeList(
        out, kbiEvents.begin(), kbiEvents.end(), [](std::ostream& out, const KbiEvent& event) {
            writeDouble(out, event.time);
            writeBool(out, event.pressed);
            writeKbiInput(out, event.input);
        });

    // Write sources
    std::list<DeviceMap::value_type> devicesList;
    devices.cvisit_all([&](const DeviceMap::value_type& device) { devicesList.push_back(device); });
    writeList(out, devicesList.begin(), devicesList.end(),
        [&](std::ostream& out, const std::pair<std::string, Device>& device) {
            writeInt64(out, indexMap[device.first]);
            inputs.cvisit(device.first,
                [&](const InputMap::value_type& input) { writeInt32(out, input.second.size()); });
            writeString(out, device.second.name);
            writeString(out, device.first);
        });

    // Write analysis parameters
    writeInt32(out, 16000); // Bin rate, default to 16000 for now
    writeInt32(out, 0); // HPS elimination
    writeBool(out, true); // Low cut

    // Write input info
    writeList(out, kbiInputInfo.begin(), kbiInputInfo.end(),
        [](std::ostream& out, const std::pair<KbiInput, KbiInputInfo>& pair) {
            const auto& [input, info] = pair;
            writeKbiInput(out, input);
            writeInt32(out, info.color);
            writeBool(out, info.visible);
        });
}

} // namespace Kbi
