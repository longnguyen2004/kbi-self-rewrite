#include "serializer.h"
#include "../helper/helper_os.h"
#include <nlohmann/json.hpp>
#include <concepts>
#include <iterator>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>

using namespace nlohmann;
namespace io = boost::iostreams;

template <typename K, typename V>
void to_json(json &j, const boost::unordered::concurrent_flat_map<K, V> &map)
{
    using map_type = std::remove_reference_t<decltype(map)>;
    map.visit_all([&](const map_type::value_type &elem)
                  {
        auto& [k, v] = elem;
        j[k] = v; });
}

void to_json(json &j, const Device &device)
{
    j = {
        {"name", device.Name},
        {"vid", device.VID},
        {"pid", device.PID}};
}
void to_json(json &j, const Input &input)
{
    j = {
        {"timestamp", input.Timestamp},
        {"pressed", input.Pressed},
        {"code", input.Code}};
}
void to_json(json &j, const Recorder &recorder)
{
    auto backend = recorder.Backend();
    // clang-format off
    j = {
        {"info", {
            {"os_name", get_os_name()},
            {"os_ver", get_os_version()},
#ifdef _WIN32
            {"safe_mode", is_safe_mode()},
#endif
            {"backend",
                backend == RecorderBackend::WINDOWS_GAMEINPUT ? "gameinput" :
                backend == RecorderBackend::LINUX_EVDEV       ? "evdev"     :
                                                                "unknown"
            }
        }},
        {"devices", recorder.Devices()},
        {"inputs", recorder.Inputs()}
    };
    // clang-format on
}

void JsonTextSerializer::Serialize(const Device &device, std::ostream &out)
{
    out << json{device};
}
void JsonTextSerializer::Serialize(const Input &input, std::ostream &out)
{
    out << json{input};
}
void JsonTextSerializer::Serialize(const Recorder &recorder, std::ostream &out)
{
    out << json{recorder};
}
std::string JsonTextSerializer::Serialize(const Device &device)
{
    return json{device};
}
std::string JsonTextSerializer::Serialize(const Input &input)
{
    return json{input};
}
std::string JsonTextSerializer::Serialize(const Recorder &recorder)
{
    return json{recorder};
}

void CborSerializer::Serialize(const Device &device, std::ostream &out)
{
    json::to_cbor(device, out);
}
void CborSerializer::Serialize(const Input &input, std::ostream &out)
{
    json::to_cbor(input, out);
}
void CborSerializer::Serialize(const Recorder &recorder, std::ostream &out)
{
    json::to_cbor(recorder, out);
}
