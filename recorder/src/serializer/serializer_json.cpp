#include "serializer.h"
#include "../helper/helper_os.h"
#include <concepts>
#include <iterator>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/json.hpp>
#include <simdutf.h>

using namespace boost::json;
namespace io = boost::iostreams;

void tag_invoke(const value_from_tag &, value &j, const UsbDeviceInfo &usbDevice)
{
    auto& val = j.emplace_object() = {
        {"vid", usbDevice.VID},
        {"pid", usbDevice.PID},
        {"speed", static_cast<std::underlying_type_t<decltype(usbDevice.Speed)>>(usbDevice.Speed)},
    };
    if (usbDevice.Descriptors.size())
    {
        auto& source = usbDevice.Descriptors;
        std::string base64(simdutf::base64_length_from_binary(source.size()), 0);
        std::ignore = simdutf::binary_to_base64(source, base64);
        val.insert_or_assign("descriptors", base64);
    }
}

void tag_invoke(const value_from_tag &, value &j, const Device &device)
{
    auto& val = j.emplace_object() = {
        {"name", device.Name},
        {"vid", device.VID},
        {"pid", device.PID}
    };
    if (device.UsbDeviceId)
        val.insert_or_assign("usb_device", device.UsbDeviceId.value());
}

void tag_invoke(const value_from_tag &, value &j, const Input &input)
{
    j.emplace_object() = {
        {"timestamp", input.Timestamp},
        {"pressed", input.Pressed},
        {"code", static_cast<std::underlying_type_t<decltype(input.Code)>>(input.Code)}
    };
}

template <typename T>
void tag_invoke(const value_from_tag &, value &j, const boost::unordered::concurrent_flat_map<std::string, T>& map)
{
    using map_type = std::remove_reference_t<decltype(map)>;
    auto& obj = j.emplace_object();
    map.cvisit_all([&](const map_type::value_type& item) {
        obj[item.first] = value_from(item.second);
    });
}

void tag_invoke(const value_from_tag &, value &j, const Recorder &recorder)
{
    auto backend = recorder.Backend();
    // clang-format off
    j.emplace_object() = {
        {"info", {
            {"os_name", get_os_name()},
            {"os_ver", get_os_version()},
            {"arch", get_arch()},
#ifdef _WIN32
            {"safe_mode", is_safe_mode()},
#endif
            {"backend",
                backend == RecorderBackend::WINDOWS_GAMEINPUT ? "gameinput" :
                backend == RecorderBackend::LINUX_EVDEV       ? "evdev"     :
                                                                "unknown"
            }
        }},
        {"time", std::format("{:%FT%TZ}", recorder.StartTime())},
        {"usb_devices", value_from(recorder.UsbDevices())},
        {"devices", value_from(recorder.Devices())},
        {"inputs", value_from(recorder.Inputs())}
    };
    // clang-format on
}

void JsonTextSerializer::Serialize(const Device &device, std::ostream &out)
{
    out << value_from(device);
}
void JsonTextSerializer::Serialize(const Input &input, std::ostream &out)
{
    out << value_from(input);
}
void JsonTextSerializer::Serialize(const Recorder &recorder, std::ostream &out)
{
    out << value_from(recorder);
}
std::string JsonTextSerializer::Serialize(const Device &device)
{
    return serialize(value_from(device));
}
std::string JsonTextSerializer::Serialize(const Input &input)
{
    return serialize(value_from(input));
}
std::string JsonTextSerializer::Serialize(const Recorder &recorder)
{
    return serialize(value_from(recorder));
}

// CBOR serializer taken from https://www.boost.org/doc/libs/latest/libs/json/doc/html/json/examples.html#json.examples.cbor
void serialize_cbor_number(
    unsigned char mt, std::uint64_t n, std::ostream& out)
{
    mt <<= 5;

    if( n < 24 )
    {
        out.put(static_cast<char>(mt + n));
    }
    else if( n < 256 )
    {
        unsigned char data[] = { static_cast<unsigned char>( mt + 24 ), static_cast<unsigned char>( n ) };
        out.write(reinterpret_cast<char*>(data), sizeof(data));
    }
    else if( n < 65536 )
    {
        unsigned char data[] = { static_cast<unsigned char>( mt + 25 ), static_cast<unsigned char>( n >> 8 ), static_cast<unsigned char>( n ) };
        out.write(reinterpret_cast<char*>(data), sizeof(data));
    }
    else if( n < 0x1000000ull )
    {
        unsigned char data[ 5 ];

        data[ 0 ] = static_cast<unsigned char>( mt + 26 );
        boost::endian::endian_store<std::uint32_t, 4, boost::endian::order::big>( data + 1, static_cast<std::uint32_t>( n ) );

        out.write(reinterpret_cast<char*>(data), sizeof(data));
    }
    else
    {
        unsigned char data[ 9 ];

        data[ 0 ] = static_cast<unsigned char>( mt + 27 );
        boost::endian::endian_store<std::uint64_t, 8, boost::endian::order::big>( data + 1, n );

        out.write(reinterpret_cast<char*>(data), sizeof(data));
    }
}

void
serialize_cbor_string( string_view sv, std::ostream& out )
{
    std::size_t n = sv.size();
    serialize_cbor_number( 3, n, out );

    out.write(sv.data(), n);
}

void
serialize_cbor_value( const value& jv, std::ostream& out )
{
    switch( jv.kind() )
    {
    case kind::null:
        out.put( 224 + 22 );
        break;

    case kind::bool_:
        out.put( 224 + 20 + jv.get_bool() );
        break;

    case kind::int64:
        {
            std::int64_t n = jv.get_int64();
            if( n >= 0 )
                serialize_cbor_number( 0, n, out );
            else
                serialize_cbor_number( 1, ~n, out );
        }
        break;

    case kind::uint64:
        serialize_cbor_number( 0, jv.get_uint64(), out );
        break;

    case kind::double_:
        {
            unsigned char data[ 9 ];
            data[ 0 ] = 224 + 27;
            boost::endian::endian_store<double, 8, boost::endian::order::big>( data + 1, jv.get_double() );

            out.write(reinterpret_cast<char*>(data), sizeof(data));
        }
        break;

    case kind::string:
        serialize_cbor_string( jv.get_string(), out );
        break;

    case kind::array:
        {
            const array& ja = jv.get_array();
            std::size_t n = ja.size();

            serialize_cbor_number( 4, n, out );

            for( std::size_t i = 0; i < n; ++i )
                serialize_cbor_value( ja[i], out );
        }
        break;

    case kind::object:
        {
            const object& jo = jv.get_object();
            std::size_t n = jo.size();

            serialize_cbor_number( 5, n, out );

            for( const key_value_pair& kv: jo )
            {
                serialize_cbor_string( kv.key(), out );
                serialize_cbor_value( kv.value(), out );
            }
        }
        break;
    }
}

void CborSerializer::Serialize(const Device &device, std::ostream &out)
{
    serialize_cbor_value(value_from(device), out);
}
void CborSerializer::Serialize(const Input &input, std::ostream &out)
{
    serialize_cbor_value(value_from(input), out);
}
void CborSerializer::Serialize(const Recorder &recorder, std::ostream &out)
{
    serialize_cbor_value(value_from(recorder), out);
}
