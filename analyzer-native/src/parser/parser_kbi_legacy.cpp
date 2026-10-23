#include "parser/parser_kbi_legacy.h"

#include "core/keycode_util.h"

#include <datetime.h>

#include <array>
#include <charconv>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

namespace Kbi {
namespace {

struct Reader {
    const std::vector<unsigned char>& buf;
    std::size_t offset = 0;

    bool has(std::size_t count) const { return offset + count <= buf.size(); }
};

std::optional<std::uint64_t> readUleb128(Reader& r)
{
    std::uint64_t result = 0;
    unsigned shift = 0;
    for (;;) {
        if (!r.has(1))
            return std::nullopt;
        const unsigned char byte = r.buf[r.offset++];
        result |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0)
            return result;
        shift += 7;
        if (shift > 63)
            return std::nullopt;
    }
}

std::optional<bool> readBool(Reader& r)
{
    if (!r.has(1))
        return std::nullopt;
    return r.buf[r.offset++] != 0;
}

std::optional<std::int32_t> readInt32(Reader& r)
{
    if (!r.has(4))
        return std::nullopt;
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(r.buf[r.offset + static_cast<std::size_t>(i)])
            << (8 * i);
    r.offset += 4;
    return static_cast<std::int32_t>(value);
}

std::optional<std::uint64_t> readUint64(Reader& r)
{
    if (!r.has(8))
        return std::nullopt;
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t>(r.buf[r.offset + static_cast<std::size_t>(i)])
            << (8 * i);
    r.offset += 8;
    return value;
}

std::optional<std::int64_t> readInt64(Reader& r)
{
    const auto value = readUint64(r);
    if (!value)
        return std::nullopt;
    return static_cast<std::int64_t>(*value);
}

std::optional<double> readDouble(Reader& r)
{
    if (!r.has(8))
        return std::nullopt;
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t>(r.buf[r.offset + static_cast<std::size_t>(i)])
            << (8 * i);
    r.offset += 8;
    double result = 0.0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

std::optional<std::string> readString(Reader& r)
{
    const std::size_t originalOffset = r.offset;
    const auto length = readUleb128(r);
    if (!length)
        return std::nullopt;
    if (!r.has(static_cast<std::size_t>(*length))) {
        r.offset = originalOffset;
        return std::nullopt;
    }
    std::string result(reinterpret_cast<const char*>(r.buf.data() + r.offset),
        static_cast<std::size_t>(*length));
    r.offset += static_cast<std::size_t>(*length);
    return result;
}

struct LegacyInput {
    std::string key;
    std::int64_t source = 0;
};

struct LegacyEvent {
    double time = 0.0;
    bool pressed = false;
    LegacyInput input;
};

struct LegacyDevice {
    std::int64_t id = 0;
    std::int32_t inputCount = 0;
    std::string name;
    std::string deviceId;
};

std::optional<LegacyInput> readKbiInput(Reader& r)
{
    const auto key = readString(r);
    if (!key)
        return std::nullopt;
    const auto source = readInt64(r);
    if (!source)
        return std::nullopt;
    LegacyInput input;
    input.key = *key;
    input.source = *source;
    return input;
}

std::optional<LegacyEvent> readKbiEvent(Reader& r)
{
    const auto time = readDouble(r);
    if (!time)
        return std::nullopt;
    const auto pressed = readBool(r);
    if (!pressed)
        return std::nullopt;
    const auto input = readKbiInput(r);
    if (!input)
        return std::nullopt;
    LegacyEvent event;
    event.time = *time;
    event.pressed = *pressed;
    event.input = *input;
    return event;
}

std::optional<LegacyDevice> readKbiDevice(Reader& r)
{
    const auto id = readInt64(r);
    if (!id)
        return std::nullopt;
    const auto inputCount = readInt32(r);
    if (!inputCount)
        return std::nullopt;
    const auto name = readString(r);
    if (!name)
        return std::nullopt;
    const auto deviceId = readString(r);
    if (!deviceId)
        return std::nullopt;
    LegacyDevice device;
    device.id = *id;
    device.inputCount = *inputCount;
    device.name = *name;
    device.deviceId = *deviceId;
    return device;
}

// Parses four hexadecimal digits, like the web analyzer does.
int parseHex4(const std::string& text, std::size_t offset)
{
    if (offset + 4 > text.size())
        return 0;
    unsigned value = 0;
    const auto result = std::from_chars(text.data() + offset, text.data() + offset + 4, value, 16);
    if (result.ec != std::errc {})
        return 0;
    return static_cast<int>(value);
}

std::size_t findToken(const std::string& text, std::string_view token)
{
    const std::size_t pos = text.find(token);
    if (pos == std::string::npos)
        return std::string::npos;
    return pos + token.size();
}

std::optional<Result> parseKbiLegacyV3(Reader& r)
{
    const auto creator = readString(r);
    if (!creator)
        return std::nullopt;
    const auto title = readString(r);
    if (!title)
        return std::nullopt;
    const auto time = readUint64(r);
    if (!time)
        return std::nullopt;
    const auto duration = readDouble(r);
    if (!duration)
        return std::nullopt;

    // Events.
    const auto eventCount = readInt32(r);
    if (!eventCount || *eventCount < 0)
        return std::nullopt;
    std::vector<LegacyEvent> events;
    events.reserve(static_cast<std::size_t>(std::min<std::int32_t>(*eventCount, 1 << 20)));
    for (std::int32_t i = 0; i < *eventCount; ++i) {
        const auto event = readKbiEvent(r);
        if (!event)
            return std::nullopt;
        events.push_back(*event);
    }

    // Sources.
    const auto sourceCount = readInt32(r);
    if (!sourceCount || *sourceCount < 0)
        return std::nullopt;
    std::vector<LegacyDevice> sources;
    for (std::int32_t i = 0; i < *sourceCount; ++i) {
        const auto device = readKbiDevice(r);
        if (!device)
            return std::nullopt;
        sources.push_back(*device);
    }

    Result result;
    result.info = RecordingInfo {};
    result.time = formatIso8601Utc(fromNetDatetimeTicks(*time), true);

    std::vector<std::string> sourceIds(sources.size());
    for (std::size_t i = 0; i < sources.size(); ++i) {
        const LegacyDevice& source = sources[i];
        sourceIds[i] = source.deviceId;

        ResultDevice device;
        device.name = source.name;
        const std::size_t vidPos = findToken(source.deviceId, "VID_");
        const std::size_t pidPos = findToken(source.deviceId, "PID_");
        if (vidPos != std::string::npos)
            device.vid = static_cast<std::uint16_t>(parseHex4(source.deviceId, vidPos));
        if (pidPos != std::string::npos)
            device.pid = static_cast<std::uint16_t>(parseHex4(source.deviceId, pidPos));
        result.addDevice(source.deviceId, device);
    }

    for (const LegacyEvent& event : events) {
        std::size_t sourceIndex = sources.size();
        for (std::size_t i = 0; i < sources.size(); ++i) {
            if (sources[i].id == event.input.source) {
                sourceIndex = i;
                break;
            }
        }
        // The web analyzer groups the events of an unknown source under the
        // "undefined" key.
        const std::string sourceId = sourceIndex < sources.size() ? sourceIds[sourceIndex] : std::string("undefined");

        const int vk = keyNameToVk(event.input.key);
        InputEvent input;
        // The legacy format stores fractional seconds.
        input.timestamp = event.time * 1000000.0;
        input.pressed = event.pressed;
        input.code = vk >= 0 ? vkToKeycode(vk) : 0;
        result.inputs[sourceId].push_back(input);
    }

    return result;
}

} // namespace

std::optional<Result> parseKbiLegacy(const std::vector<unsigned char>& buffer)
{
    static constexpr std::array<char, 4> header = { 'K', 'B', 'I', '\0' };
    if (buffer.size() < 4)
        return std::nullopt;
    for (int i = 0; i < 4; ++i) {
        if (buffer[static_cast<std::size_t>(i)] != static_cast<unsigned char>(header[i]))
            return std::nullopt;
    }

    Reader reader { .buf = buffer, .offset = 4 };
    const auto version = readInt32(reader);
    if (!version)
        return std::nullopt;
    if (*version == 3)
        return parseKbiLegacyV3(reader);
    return std::nullopt;
}

} // namespace Kbi
