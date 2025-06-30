#pragma once
#include "../system/info.h"
#include <recorder.h>
#include <ostream>

#define DECLARE_OSTREAM_SERIALIZER \
    virtual void Serialize(const UsbDeviceInfo& device, std::ostream& out) PURE; \
    virtual void Serialize(const Device& device, std::ostream& out) PURE;        \
    virtual void Serialize(const Input& input, std::ostream& out) PURE;          \
    virtual void Serialize(const Recorder& recorder, std::ostream& out) PURE;    \
    virtual void Serialize(const SystemInfo& sysInfo, std::ostream& out) PURE;

#define DECLARE_STRING_SERIALIZER \
    virtual std::string Serialize(const UsbDeviceInfo& device); \
    virtual std::string Serialize(const Device& device);        \
    virtual std::string Serialize(const Input& input);          \
    virtual std::string Serialize(const Recorder& recorder);    \
    virtual std::string Serialize(const SystemInfo& sysInfo);

#define DECLARE_BUFFER_SERIALIZER \
    virtual void Serialize(const UsbDeviceInfo& device, char* out, size_t n); \
    virtual void Serialize(const Device& device, char* out, size_t n);        \
    virtual void Serialize(const Input& input, char* out, size_t n);          \
    virtual void Serialize(const Recorder& recorder, char* out, size_t n);    \
    virtual void Serialize(const SystemInfo& sysInfo, char* out, size_t n);

class Serializer
{
public:
#define PURE =0
    DECLARE_OSTREAM_SERIALIZER
#undef PURE
    DECLARE_STRING_SERIALIZER
    DECLARE_BUFFER_SERIALIZER
};

class JsonTextSerializer: public Serializer
{
public:
#define PURE
    DECLARE_OSTREAM_SERIALIZER
#undef PURE
    DECLARE_STRING_SERIALIZER
};

class CborSerializer: public Serializer
{
public:
#define PURE
    DECLARE_OSTREAM_SERIALIZER
#undef PURE
};
