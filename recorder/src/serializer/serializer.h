#pragma once

#include <recorder.h>
#include <system_info.h>
#include <boost/json/fwd.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <ostream>
#include <span>
#include <string>

namespace Kbi {

// The `spec` macro argument carries the trailing declaration specifier of the
// generated functions: `= 0` for the pure virtual interface functions and
// `override` in the derived classes (C.128).
#define SERIALIZER_CLASS_TO_DECLARE (UsbDeviceInfo)(Device)(Input)(Recorder)(SystemInfo)

#define DECLARE_OSTREAM_SERIALIZER(r, spec, type) \
    virtual void serialize(const type& a, std::ostream& out) const spec;
#define DECLARE_STRING_SERIALIZER(r, spec, type) \
    virtual std::string serialize(const type& val) const spec;
#define DECLARE_BUFFER_SERIALIZER(r, spec, type) \
    virtual void serialize(const type& val, std::span<char> out) const spec;

#define DECLARE_OSTREAM_SERIALIZER_ALL(spec) \
    BOOST_PP_SEQ_FOR_EACH(DECLARE_OSTREAM_SERIALIZER, spec, SERIALIZER_CLASS_TO_DECLARE)
#define DECLARE_STRING_SERIALIZER_ALL(spec) \
    BOOST_PP_SEQ_FOR_EACH(DECLARE_STRING_SERIALIZER, spec, SERIALIZER_CLASS_TO_DECLARE)
#define DECLARE_BUFFER_SERIALIZER_ALL(spec) \
    BOOST_PP_SEQ_FOR_EACH(DECLARE_BUFFER_SERIALIZER, spec, SERIALIZER_CLASS_TO_DECLARE)

class Serializer {
public:
    virtual ~Serializer() = default;

    DECLARE_OSTREAM_SERIALIZER_ALL(= 0)
    DECLARE_STRING_SERIALIZER_ALL()
    DECLARE_BUFFER_SERIALIZER_ALL()
};

class JsonTextSerializer : public Serializer {
public:
    DECLARE_OSTREAM_SERIALIZER_ALL(override)
    DECLARE_STRING_SERIALIZER_ALL(override)

#define DECLARE_TO_JSON(r, _, type) boost::json::value toJson(const type& val) const;

    BOOST_PP_SEQ_FOR_EACH(DECLARE_TO_JSON, _, SERIALIZER_CLASS_TO_DECLARE)

#undef DECLARE_TO_JSON
};

class CborSerializer : public Serializer {
public:
    DECLARE_OSTREAM_SERIALIZER_ALL(override)
};

} // namespace Kbi
