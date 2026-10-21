#include "parser/parser.h"

#include "parser/parser_kbi_legacy.h"
#include "parser/usb_descriptors.h"

#include <boost/json.hpp>
#include <boost/system/system_error.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>

namespace kbi {
namespace {

namespace json = boost::json;

std::string get_string(const json::object& object, std::string_view key,
                       std::string fallback = {}) {
    const auto it = object.find(key);
    if (it == object.end() || !it->value().is_string())
        return fallback;
    return std::string(it->value().as_string());
}

std::optional<std::int64_t> get_int(const json::object& object, std::string_view key) {
    const auto it = object.find(key);
    if (it == object.end())
        return std::nullopt;
    const json::value& value = it->value();
    if (value.is_int64())
        return value.as_int64();
    if (value.is_uint64())
        return static_cast<std::int64_t>(value.as_uint64());
    if (value.is_double())
        return static_cast<std::int64_t>(value.as_double());
    return std::nullopt;
}

std::optional<double> get_double(const json::object& object, std::string_view key) {
    const auto it = object.find(key);
    if (it == object.end())
        return std::nullopt;
    const json::value& value = it->value();
    if (value.is_number())
        return value.to_number<double>();
    return std::nullopt;
}

UsbDeviceSpeed parse_speed(std::int64_t value) {
    switch (value) {
    case 1:
        return UsbDeviceSpeed::LowSpeed;
    case 2:
        return UsbDeviceSpeed::FullSpeed;
    case 3:
        return UsbDeviceSpeed::HighSpeed;
    case 4:
        return UsbDeviceSpeed::SuperSpeed;
    default:
        return UsbDeviceSpeed::Unknown;
    }
}

} // namespace

std::optional<Result> parse_result_json(const std::string& json_text) {
    json::value root;
    try {
        root = json::parse(json_text);
    } catch (const boost::system::system_error&) {
        return std::nullopt;
    }
    if (!root.is_object())
        return std::nullopt;

    const json::object& object = root.as_object();
    Result result;

    if (const auto it = object.find("info"); it != object.end() && it->value().is_object()) {
        const json::object& info = it->value().as_object();
        result.info.os_name = get_string(info, "os_name");
        result.info.os_ver = get_string(info, "os_ver");
        result.info.arch = get_string(info, "arch");
        result.info.backend = get_string(info, "backend");
        if (const auto safe_mode = info.find("safe_mode");
            safe_mode != info.end() && safe_mode->value().is_bool())
            result.info.safe_mode = safe_mode->value().as_bool();
    }

    result.time = get_string(object, "time");

    if (const auto it = object.find("usb_devices"); it != object.end() && it->value().is_object()) {
        for (const auto& [id, node] : it->value().as_object()) {
            if (!node.is_object())
                continue;
            const json::object& entry = node.as_object();
            UsbDevice device;
            if (const auto vid = get_int(entry, "vid"))
                device.vid = static_cast<std::uint16_t>(*vid);
            if (const auto pid = get_int(entry, "pid"))
                device.pid = static_cast<std::uint16_t>(*pid);
            if (const auto speed = get_int(entry, "speed"))
                device.speed = parse_speed(*speed);
            if (const auto descriptors = entry.find("descriptors");
                descriptors != entry.end() && descriptors->value().is_string()) {
                const std::string decoded =
                    base64_decode(std::string(descriptors->value().as_string()));
                device.descriptors.assign(decoded.begin(), decoded.end());
            }
            result.usb_devices[id] = std::move(device);
        }
    }

    if (const auto it = object.find("devices"); it != object.end() && it->value().is_object()) {
        for (const auto& [id, node] : it->value().as_object()) {
            if (!node.is_object())
                continue;
            const json::object& entry = node.as_object();
            Device device;
            device.name = get_string(entry, "name");
            if (const auto vid = get_int(entry, "vid"))
                device.vid = static_cast<std::uint16_t>(*vid);
            if (const auto pid = get_int(entry, "pid"))
                device.pid = static_cast<std::uint16_t>(*pid);
            if (const auto usb = entry.find("usb_device");
                usb != entry.end() && usb->value().is_string())
                device.usb_device = std::string(usb->value().as_string());
            result.devices[id] = std::move(device);
        }
    }

    if (const auto it = object.find("inputs"); it != object.end() && it->value().is_object()) {
        for (const auto& [id, node] : it->value().as_object()) {
            if (!node.is_array())
                continue;
            auto& events = result.inputs[id];
            const json::array& array = node.as_array();
            events.reserve(array.size());
            for (const json::value& entry : array) {
                if (!entry.is_object())
                    continue;
                const json::object& event = entry.as_object();
                InputEvent input;
                if (const auto timestamp = get_double(event, "timestamp"))
                    input.timestamp = std::max(0.0, *timestamp);
                if (const auto pressed = event.find("pressed");
                    pressed != event.end() && pressed->value().is_bool())
                    input.pressed = pressed->value().as_bool();
                if (const auto code = get_int(event, "code"))
                    input.code = static_cast<int>(*code);
                events.push_back(input);
            }
        }
    }

    return result;
}

std::optional<std::string> read_file(const std::string& path) {
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    if (!stream)
        return std::nullopt;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::optional<Result> parse_result_file(const std::string& path) {
    const auto contents = read_file(path);
    if (!contents)
        return std::nullopt;

    // Skip leading whitespace to decide between JSON and the binary format.
    std::size_t start = 0;
    while (start < contents->size() &&
           std::isspace(static_cast<unsigned char>((*contents)[start])))
        ++start;

    if (start < contents->size() && (*contents)[start] == '{')
        return parse_result_json(*contents);

    const std::vector<unsigned char> bytes(contents->begin(), contents->end());
    return parse_kbi_legacy(bytes);
}

} // namespace kbi
