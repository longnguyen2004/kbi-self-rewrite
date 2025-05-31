#include "serializer.h"
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <sstream>

namespace io = boost::iostreams;

std::string Serializer::Serialize(const Device& device)
{
    std::ostringstream os;
    Serialize(device, os);
    return os.str();
}
std::string Serializer::Serialize(const Input& input)
{
    std::ostringstream os;
    Serialize(input, os);
    return os.str();
}
std::string Serializer::Serialize(const Recorder& recorder)
{
    std::ostringstream os;
    Serialize(recorder, os);
    return os.str();
}
void Serializer::Serialize(const Device& device, char* out, size_t n)
{
    io::stream<io::array_sink> os(out, n);
    Serialize(device, os);
}
void Serializer::Serialize(const Input& input, char* out, size_t n)
{
    io::stream<io::array_sink> os(out, n);
    Serialize(input, os);
}
void Serializer::Serialize(const Recorder& recorder, char* out, size_t n)
{
    io::stream<io::array_sink> os(out, n);
    Serialize(recorder, os);
}