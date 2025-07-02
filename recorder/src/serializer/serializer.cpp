#include "serializer.h"
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <sstream>

namespace io = boost::iostreams;

#define DEFINE_SERIALIZE_WRAPPER(r, _, type) \
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

BOOST_PP_SEQ_FOR_EACH(DEFINE_SERIALIZE_WRAPPER, _, SERIALIZER_CLASS_TO_DECLARE)
