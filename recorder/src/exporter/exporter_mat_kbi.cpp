#include "exporter.h"
#include <datetime.h>
#include <keycode.h>
#include <array>
#include <boost/endian.hpp>
#include <chrono>
#include <cstdint>
#include <iterator>
#include <map>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace Kbi {

using namespace boost::endian;
using namespace std::chrono;

namespace {

// Microseconds per second; input timestamps are in microseconds.
constexpr double kMicrosPerSecond = 1'000'000.0;

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

const std::array<char, 4> kbiHeader = { 'K', 'B', 'I', 0 };
constexpr std::string_view creator = "kbi_cpp_recorder";

bool operator==(const KbiInput& a, const KbiInput& b)
{
    return a.key == b.key && a.source == b.source;
}

bool operator<(const KbiInput& a, const KbiInput& b)
{
    return a.key != b.key ? a.key < b.key : a.source < b.source;
}

void writeUleb128(std::ostream& out, std::uint64_t x)
{
    // ULEB128 always writes at least one byte (ES.75 — no do-statement).
    while (true) {
        auto byte = x & 0x7FU;
        x >>= 7;
        if (x != 0)
            byte |= 0x80U;
        out.put(static_cast<char>(byte));
        if (x == 0)
            break;
    }
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
    const auto xBits = std::bit_cast<std::uint64_t>(x);
    writeUint64(out, xBits);
}

void writeString(std::ostream& out, std::string_view str)
{
    writeUleb128(out, str.size());
    out.write(str.data(), str.size());
}

template<typename Iterator, typename Serializer>
    requires std::forward_iterator<Iterator>
void writeList(std::ostream& out, Iterator start, Iterator end, Serializer serializer)
{
    const auto count = static_cast<std::int32_t>(std::distance(start, end));
    writeInt32(out, count);
    for (auto it = start; it != end; ++it)
        serializer(out, *it);
}

void writeTime(std::ostream& out, time_point<system_clock> t)
{
    // The .NET DateTime binary format, see datetime.h.
    writeUint64(out, toNetDatetimeTicks(t, kNetDateTimeKindUtc));
}

void writeKbiInput(std::ostream& out, const KbiInput& input)
{
    writeString(out, input.key);
    writeInt64(out, input.source);
}

} // namespace

ExporterMatKbi::ExporterMatKbi(const Recorder& recorder)
    : m_recorder(recorder)
{
}

void ExporterMatKbi::exportTo(std::ostream& out) const
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
    std::vector<KbiEvent> kbiEvents;
    std::map<KbiInput, KbiInputInfo> kbiInputInfo;

    inputs.visit_all([&](const InputMap::value_type& device) {
        const auto& [id, events] = device;
        for (const auto& event : events) {
            KbiInput input { .key = std::string { keycodeToString(event.code) },
                .source = indexMap[id] };
            kbiEvents.emplace_back(event.timestamp / kMicrosPerSecond, event.pressed, input);
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
    std::vector<DeviceMap::value_type> devicesList;
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
        [](std::ostream& out, const std::pair<const KbiInput, KbiInputInfo>& pair) {
            const auto& [input, info] = pair;
            writeKbiInput(out, input);
            writeInt32(out, info.color);
            writeBool(out, info.visible);
        });
}

} // namespace Kbi
