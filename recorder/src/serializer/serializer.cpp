#include "serializer.h"
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <sstream>

namespace io = boost::iostreams;

#define DEFINE_SERIALIZE_WRAPPER(type) \
    std::string Serializer::Serialize(const type& a)    \
    {                                                   \
        std::ostringstream os;                          \
        Serialize(a, os);                               \
        return os.str();                                \
    }                                                   \
    void Serializer::Serialize(const type& a, char* out, size_t n)  \
    {                                                               \
        io::stream<io::array_sink> os(out, n);                      \
        Serialize(a, os);                                           \
    }

DEFINE_SERIALIZE_WRAPPER(UsbDeviceInfo)
DEFINE_SERIALIZE_WRAPPER(Device)
DEFINE_SERIALIZE_WRAPPER(Input)
DEFINE_SERIALIZE_WRAPPER(Recorder)
DEFINE_SERIALIZE_WRAPPER(SystemInfo)
