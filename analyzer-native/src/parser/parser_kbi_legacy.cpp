#include "parser/parser_kbi_legacy.h"

#include "core/keycode_util.h"

#include <datetime.h>

#include <charconv>
#include <cstring>
#include <optional>
#include <string>

namespace kbi {
namespace {

struct Reader {
    const std::vector<unsigned char>& buf;
    std::size_t offset = 0;

    bool has(std::size_t count) const { return offset + count <= buf.size(); }
};

std::optional<std::uint64_t> read_uleb128(Reader& r) {
    std::uint64_t result = 0;
    unsigned shift = 0;
    for (;;) {
        if (!r.has(1))
            return std::nullopt;
        const unsigned char byte = r.buf[r.offset++];
        result |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
        if (!(byte & 0x80))
            return result;
        shift += 7;
        if (shift > 63)
            return std::nullopt;
    }
}

std::optional<bool> read_bool(Reader& r) {
    if (!r.has(1))
        return std::nullopt;
    return r.buf[r.offset++] != 0;
}

std::optional<std::int32_t> read_int32(Reader& r) {
    if (!r.has(4))
        return std::nullopt;
    std::uint32_t value = 0;
    for (int i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(r.buf[r.offset + static_cast<std::size_t>(i)])
                 << (8 * i);
    r.offset += 4;
    return static_cast<std::int32_t>(value);
}

std::optional<std::uint64_t> read_uint64(Reader& r) {
    if (!r.has(8))
        return std::nullopt;
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i)
        value |= static_cast<std::uint64_t>(r.buf[r.offset + static_cast<std::size_t>(i)])
                 << (8 * i);
    r.offset += 8;
    return value;
}

std::optional<std::int64_t> read_int64(Reader& r) {
    const auto value = read_uint64(r);
    if (!value)
        return std::nullopt;
    return static_cast<std::int64_t>(*value);
}

std::optional<double> read_double(Reader& r) {
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

std::optional<std::string> read_string(Reader& r) {
    const std::size_t original_offset = r.offset;
    const auto length = read_uleb128(r);
    if (!length)
        return std::nullopt;
    if (!r.has(static_cast<std::size_t>(*length))) {
        r.offset = original_offset;
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
    std::int32_t input_count = 0;
    std::string name;
    std::string device_id;
};

std::optional<LegacyInput> read_kbi_input(Reader& r) {
    const auto key = read_string(r);
    if (!key)
        return std::nullopt;
    const auto source = read_int64(r);
    if (!source)
        return std::nullopt;
    LegacyInput input;
    input.key = *key;
    input.source = *source;
    return input;
}

std::optional<LegacyEvent> read_kbi_event(Reader& r) {
    const auto time = read_double(r);
    if (!time)
        return std::nullopt;
    const auto pressed = read_bool(r);
    if (!pressed)
        return std::nullopt;
    const auto input = read_kbi_input(r);
    if (!input)
        return std::nullopt;
    LegacyEvent event;
    event.time = *time;
    event.pressed = *pressed;
    event.input = *input;
    return event;
}

std::optional<LegacyDevice> read_kbi_device(Reader& r) {
    const auto id = read_int64(r);
    if (!id)
        return std::nullopt;
    const auto input_count = read_int32(r);
    if (!input_count)
        return std::nullopt;
    const auto name = read_string(r);
    if (!name)
        return std::nullopt;
    const auto device_id = read_string(r);
    if (!device_id)
        return std::nullopt;
    LegacyDevice device;
    device.id = *id;
    device.input_count = *input_count;
    device.name = *name;
    device.device_id = *device_id;
    return device;
}

// Parses four hexadecimal digits, like the web analyzer does.
int parse_hex4(const std::string& text, std::size_t offset) {
    if (offset + 4 > text.size())
        return 0;
    unsigned value = 0;
    const auto result = std::from_chars(text.data() + offset, text.data() + offset + 4, value, 16);
    if (result.ec != std::errc{})
        return 0;
    return static_cast<int>(value);
}

std::size_t find_token(const std::string& text, const char* token) {
    const std::size_t pos = text.find(token);
    if (pos == std::string::npos)
        return std::string::npos;
    return pos + std::char_traits<char>::length(token);
}

std::optional<Result> parse_kbi_legacy_v3(Reader& r) {
    const auto creator = read_string(r);
    if (!creator)
        return std::nullopt;
    const auto title = read_string(r);
    if (!title)
        return std::nullopt;
    const auto time = read_uint64(r);
    if (!time)
        return std::nullopt;
    const auto duration = read_double(r);
    if (!duration)
        return std::nullopt;

    // Events.
    const auto event_count = read_int32(r);
    if (!event_count || *event_count < 0)
        return std::nullopt;
    std::vector<LegacyEvent> events;
    events.reserve(static_cast<std::size_t>(std::min<std::int32_t>(*event_count, 1 << 20)));
    for (std::int32_t i = 0; i < *event_count; ++i) {
        const auto event = read_kbi_event(r);
        if (!event)
            return std::nullopt;
        events.push_back(*event);
    }

    // Sources.
    const auto source_count = read_int32(r);
    if (!source_count || *source_count < 0)
        return std::nullopt;
    std::vector<LegacyDevice> sources;
    for (std::int32_t i = 0; i < *source_count; ++i) {
        const auto device = read_kbi_device(r);
        if (!device)
            return std::nullopt;
        sources.push_back(*device);
    }

    Result result;
    result.info = RecordingInfo{};
    result.time = format_iso8601_utc(from_net_datetime_ticks(*time), true);

    std::vector<std::string> source_ids(sources.size());
    for (std::size_t i = 0; i < sources.size(); ++i) {
        const LegacyDevice& source = sources[i];
        source_ids[i] = source.device_id;

        Device device;
        device.name = source.name;
        const std::size_t vid_pos = find_token(source.device_id, "VID_");
        const std::size_t pid_pos = find_token(source.device_id, "PID_");
        if (vid_pos != std::string::npos)
            device.vid = static_cast<std::uint16_t>(parse_hex4(source.device_id, vid_pos));
        if (pid_pos != std::string::npos)
            device.pid = static_cast<std::uint16_t>(parse_hex4(source.device_id, pid_pos));
        result.devices[source.device_id] = device;
    }

    for (const LegacyEvent& event : events) {
        std::size_t source_index = sources.size();
        for (std::size_t i = 0; i < sources.size(); ++i) {
            if (sources[i].id == event.input.source) {
                source_index = i;
                break;
            }
        }
        // The web analyzer groups the events of an unknown source under the
        // "undefined" key.
        const std::string source_id =
            source_index < sources.size() ? source_ids[source_index] : std::string("undefined");

        const int vk = key_name_to_vk(event.input.key);
        InputEvent input;
        // The legacy format stores fractional seconds.
        input.timestamp = event.time * 1000000.0;
        input.pressed = event.pressed;
        input.code = vk >= 0 ? vk_to_keycode(vk) : 0;
        result.inputs[source_id].push_back(input);
    }

    return result;
}

} // namespace

std::optional<Result> parse_kbi_legacy(const std::vector<unsigned char>& buffer) {
    static const char header[4] = {'K', 'B', 'I', '\0'};
    if (buffer.size() < 4)
        return std::nullopt;
    for (int i = 0; i < 4; ++i) {
        if (buffer[static_cast<std::size_t>(i)] != static_cast<unsigned char>(header[i]))
            return std::nullopt;
    }

    Reader reader{buffer, 4};
    const auto version = read_int32(reader);
    if (!version)
        return std::nullopt;
    switch (*version) {
    case 3:
        return parse_kbi_legacy_v3(reader);
    default:
        return std::nullopt;
    }
}

} // namespace kbi
