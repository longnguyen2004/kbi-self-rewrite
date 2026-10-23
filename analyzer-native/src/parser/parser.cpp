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

namespace Kbi {
namespace {

namespace json = boost::json;

std::string getString(const json::object& object, std::string_view key,
    std::string fallback = {})
{
    const auto* const it = object.find(key);
    if (it == object.end() || !it->value().is_string())
        return fallback;
    return std::string(it->value().as_string());
}

std::optional<std::int64_t> getInt(const json::object& object, std::string_view key)
{
    const auto* const it = object.find(key);
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

std::optional<double> getDouble(const json::object& object, std::string_view key)
{
    const auto* const it = object.find(key);
    if (it == object.end())
        return std::nullopt;
    const json::value& value = it->value();
    if (value.is_number())
        return value.to_number<double>();
    return std::nullopt;
}

UsbDeviceSpeed parseSpeed(std::int64_t value)
{
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

void parseInfo(const json::object& object, Result& result)
{
    const auto* const it = object.find("info");
    if (it == object.end() || !it->value().is_object())
        return;

    const json::object& info = it->value().as_object();
    result.info.osName = getString(info, "os_name");
    result.info.osVer = getString(info, "os_ver");
    result.info.arch = getString(info, "arch");
    result.info.backend = getString(info, "backend");
    if (const auto* const safeMode = info.find("safe_mode");
        safeMode != info.end() && safeMode->value().is_bool())
        result.info.safeMode = safeMode->value().as_bool();
}

void parseUsbDevices(const json::object& object, Result& result)
{
    const auto* const it = object.find("usb_devices");
    if (it == object.end() || !it->value().is_object())
        return;

    for (const auto& [id, node] : it->value().as_object()) {
        if (!node.is_object())
            continue;
        const json::object& entry = node.as_object();
        UsbDevice device;
        if (const auto vid = getInt(entry, "vid"))
            device.vid = static_cast<std::uint16_t>(*vid);
        if (const auto pid = getInt(entry, "pid"))
            device.pid = static_cast<std::uint16_t>(*pid);
        if (const auto speed = getInt(entry, "speed"))
            device.speed = parseSpeed(*speed);
        if (const auto* const descriptors = entry.find("descriptors");
            descriptors != entry.end() && descriptors->value().is_string()) {
            const std::string decoded = base64Decode(std::string(descriptors->value().as_string()));
            device.descriptors.assign(decoded.begin(), decoded.end());
        }
        result.usbDevices[id] = std::move(device);
    }
}

void parseDevices(const json::object& object, Result& result)
{
    const auto* const it = object.find("devices");
    if (it == object.end() || !it->value().is_object())
        return;

    for (const auto& [id, node] : it->value().as_object()) {
        if (!node.is_object())
            continue;
        const json::object& entry = node.as_object();
        ResultDevice device;
        device.name = getString(entry, "name");
        if (const auto vid = getInt(entry, "vid"))
            device.vid = static_cast<std::uint16_t>(*vid);
        if (const auto pid = getInt(entry, "pid"))
            device.pid = static_cast<std::uint16_t>(*pid);
        if (const auto* const usb = entry.find("usb_device");
            usb != entry.end() && usb->value().is_string())
            device.usbDevice = std::string(usb->value().as_string());
        result.addDevice(id, std::move(device));
    }
}

void parseInputs(const json::object& object, Result& result)
{
    const auto* const it = object.find("inputs");
    if (it == object.end() || !it->value().is_object())
        return;

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
            if (const auto timestamp = getDouble(event, "timestamp"))
                input.timestamp = std::max(0.0, *timestamp);
            if (const auto* const pressed = event.find("pressed");
                pressed != event.end() && pressed->value().is_bool())
                input.pressed = pressed->value().as_bool();
            if (const auto code = getInt(event, "code"))
                input.code = static_cast<int>(*code);
            events.push_back(input);
        }
    }
}

} // namespace

std::optional<Result> parseResultJson(const std::string& jsonText)
{
    json::value root;
    try {
        root = json::parse(jsonText);
    } catch (const boost::system::system_error&) {
        return std::nullopt;
    }
    if (!root.is_object())
        return std::nullopt;

    const json::object& object = root.as_object();
    Result result;

    parseInfo(object, result);
    result.time = getString(object, "time");
    parseUsbDevices(object, result);
    parseDevices(object, result);
    parseInputs(object, result);

    return result;
}

std::optional<std::string> readFile(const std::string& path)
{
    std::ifstream stream(path, std::ios::in | std::ios::binary);
    if (!stream)
        return std::nullopt;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::optional<Result> parseResultFile(const std::string& path)
{
    const auto contents = readFile(path);
    if (!contents)
        return std::nullopt;

    // Skip leading whitespace to decide between JSON and the binary format.
    std::size_t start = 0;
    while (start < contents->size() && (std::isspace(static_cast<unsigned char>((*contents)[start])) != 0))
        ++start;

    if (start < contents->size() && (*contents)[start] == '{')
        return parseResultJson(*contents);

    const std::vector<unsigned char> bytes(contents->begin(), contents->end());
    return parseKbiLegacy(bytes);
}

} // namespace Kbi
