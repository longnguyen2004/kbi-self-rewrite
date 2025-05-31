#pragma once
#include <recorder.h>
#include <ostream>

class Serializer
{
public:
    virtual void Serialize(const Device& device, std::ostream& out) = 0;
    virtual void Serialize(const Input& input, std::ostream& out) = 0;
    virtual void Serialize(const Recorder& recorder, std::ostream& out) = 0;
    virtual std::string Serialize(const Device& device);
    virtual std::string Serialize(const Input& input);
    virtual std::string Serialize(const Recorder& recorder);
    virtual void Serialize(const Device& device, char* out, size_t n);
    virtual void Serialize(const Input& input, char* out, size_t n);
    virtual void Serialize(const Recorder& recorder, char* out, size_t n);
};

class JsonTextSerializer: public Serializer
{
public:
    virtual void Serialize(const Device& device, std::ostream& out);
    virtual void Serialize(const Input& input, std::ostream& out);
    virtual void Serialize(const Recorder& recorder, std::ostream& out);
    virtual std::string Serialize(const Device& device);
    virtual std::string Serialize(const Input& input);
    virtual std::string Serialize(const Recorder& recorder);
};

class CborSerializer: public Serializer
{
public:
    virtual void Serialize(const Device& device, std::ostream& out);
    virtual void Serialize(const Input& input, std::ostream& out);
    virtual void Serialize(const Recorder& recorder, std::ostream& out);
};
