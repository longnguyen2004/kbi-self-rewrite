#include "serializer.h"
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <sstream>

namespace io = boost::iostreams;

namespace Kbi {

#define DEFINE_SERIALIZE_WRAPPER(r, _, type)                            \
    std::string Serializer::serialize(const type& a)                    \
    {                                                                   \
        std::ostringstream os;                                          \
        serialize(a, os);                                               \
        return os.str();                                                \
    }                                                                   \
    void Serializer::serialize(const type& a, char* out, std::size_t n) \
    {                                                                   \
        io::stream<io::array_sink> os(out, n);                          \
        serialize(a, os);                                               \
    }

BOOST_PP_SEQ_FOR_EACH(DEFINE_SERIALIZE_WRAPPER, _, SERIALIZER_CLASS_TO_DECLARE)

} // namespace Kbi
