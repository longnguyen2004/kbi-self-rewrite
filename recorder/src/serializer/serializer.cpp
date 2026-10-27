#include "serializer.h"
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <span>
#include <sstream>

namespace io = boost::iostreams;

namespace Kbi {

// The base implements the string and the buffer variants on top of the
// streaming interface, once per type.
std::string Serializer::serialize(const UsbDeviceInfo& a) const
{
    std::ostringstream os;
    serialize(a, os);
    return os.str();
}

std::string Serializer::serialize(const Device& a) const
{
    std::ostringstream os;
    serialize(a, os);
    return os.str();
}

std::string Serializer::serialize(const Input& a) const
{
    std::ostringstream os;
    serialize(a, os);
    return os.str();
}

std::string Serializer::serialize(const Recorder& a) const
{
    std::ostringstream os;
    serialize(a, os);
    return os.str();
}

std::string Serializer::serialize(const SystemInfo& a) const
{
    std::ostringstream os;
    serialize(a, os);
    return os.str();
}

void Serializer::serialize(const UsbDeviceInfo& a, std::span<char> out) const
{
    io::stream<io::array_sink> os(out.data(), out.size());
    serialize(a, os);
}

void Serializer::serialize(const Device& a, std::span<char> out) const
{
    io::stream<io::array_sink> os(out.data(), out.size());
    serialize(a, os);
}

void Serializer::serialize(const Input& a, std::span<char> out) const
{
    io::stream<io::array_sink> os(out.data(), out.size());
    serialize(a, os);
}

void Serializer::serialize(const Recorder& a, std::span<char> out) const
{
    io::stream<io::array_sink> os(out.data(), out.size());
    serialize(a, os);
}

void Serializer::serialize(const SystemInfo& a, std::span<char> out) const
{
    io::stream<io::array_sink> os(out.data(), out.size());
    serialize(a, os);
}

} // namespace Kbi
