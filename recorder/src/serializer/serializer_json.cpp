#include "serializer.h"
#include "../system/helper_os.h"
#include "../system/info.h"
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

void tag_invoke(const value_from_tag &, value &j, const SystemInfo& sysInfo)
{
    // clang-format off
    j.emplace_object() = {
#if defined(_WIN32)
        {"os", "windows"},
        {"os_name", sysInfo.Common.OsName},
        {"os_ver", sysInfo.Common.OsVersion},
        {"arch", sysInfo.Common.Architecture},
        {"cpu", sysInfo.Common.CpuName},
        {"safe_mode", sysInfo.IsSafeMode},
        {"clock_freq", sysInfo.ClockFrequency},
#elif defined(__linux__)
        {"os", "linux"},
        {"os_name", sysInfo.Common.OsName},
        {"os_ver", sysInfo.Common.OsVersion},
        {"arch", sysInfo.Common.Architecture},
        {"cpu", sysInfo.Common.CpuName},
        {"distro_name", sysInfo.DistroName},
        {"distro_ver", sysInfo.DistroVersion},
        {"clock_source", sysInfo.ClockSource},
        {"clock_freq", sysInfo.ClockFrequency}
#endif
    };
    // clang-format on
}

void tag_invoke(const value_from_tag &, value &j, const Recorder &recorder)
{
    auto backend = recorder.Backend();
    auto sysInfo = value_from(GetSystemInfo()).as_object();
    // clang-format off
    sysInfo["backend"] =
        backend == RecorderBackend::WINDOWS_GAMEINPUT ? "gameinput" :
        backend == RecorderBackend::LINUX_EVDEV       ? "evdev"     :
                                                        "unknown";
    j.emplace_object() = {
        {"info", sysInfo},
        {"time", std::format("{:%FT%TZ}", recorder.StartTime())},
        {"usb_devices", value_from(recorder.UsbDevices())},
        {"devices", value_from(recorder.Devices())},
        {"inputs", value_from(recorder.Inputs())}
    };
    // clang-format on
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

#define DEFINE_JSON_SERIALIZER(r, _, type) \
void JsonTextSerializer::Serialize(const type& a, std::ostream& out)    \
{                                                                       \
    out << value_from(a);                                               \
}                                                                       \
std::string JsonTextSerializer::Serialize(const type& a)    \
{                                                           \
    return serialize(value_from(a));                        \
}                                                           \
void CborSerializer::Serialize(const type& a, std::ostream& out)    \
{                                                                   \
    serialize_cbor_value(value_from(a), out);                       \
}

BOOST_PP_SEQ_FOR_EACH(DEFINE_JSON_SERIALIZER, _, SERIALIZER_CLASS_TO_DECLARE)
