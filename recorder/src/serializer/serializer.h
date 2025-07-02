#pragma once
#include "../system/info.h"
#include <recorder.h>
#include <boost/preprocessor/seq/for_each.hpp>
#include <ostream>

#define SERIALIZER_CLASS_TO_DECLARE (UsbDeviceInfo)(Device)(Input)(Recorder)(SystemInfo)

#define DECLARE_OSTREAM_SERIALIZER(r, pure, type) \
    virtual void Serialize(const type& a, std::ostream& out) BOOST_PP_IF(pure, =0,);
#define DECLARE_STRING_SERIALIZER(r, pure, type) \
    virtual std::string Serialize(const type& device) BOOST_PP_IF(pure, =0,);
#define DECLARE_BUFFER_SERIALIZER(r, pure, type) \
    virtual void Serialize(const type& device, char* out, size_t n) BOOST_PP_IF(pure, =0,);

#define DECLARE_OSTREAM_SERIALIZER_ALL(pure) \
    BOOST_PP_SEQ_FOR_EACH(DECLARE_OSTREAM_SERIALIZER, pure, SERIALIZER_CLASS_TO_DECLARE)
#define DECLARE_STRING_SERIALIZER_ALL(pure) \
    BOOST_PP_SEQ_FOR_EACH(DECLARE_STRING_SERIALIZER, pure, SERIALIZER_CLASS_TO_DECLARE)
#define DECLARE_BUFFER_SERIALIZER_ALL(pure) \
    BOOST_PP_SEQ_FOR_EACH(DECLARE_BUFFER_SERIALIZER, pure, SERIALIZER_CLASS_TO_DECLARE)

class Serializer
{
public:
    DECLARE_OSTREAM_SERIALIZER_ALL(1)
    DECLARE_STRING_SERIALIZER_ALL(0)
    DECLARE_BUFFER_SERIALIZER_ALL(0)
};

class JsonTextSerializer: public Serializer
{
public: 
    DECLARE_OSTREAM_SERIALIZER_ALL(0)
    DECLARE_STRING_SERIALIZER_ALL(0)
};

class CborSerializer: public Serializer
{
public:
    DECLARE_OSTREAM_SERIALIZER_ALL(0)
};
