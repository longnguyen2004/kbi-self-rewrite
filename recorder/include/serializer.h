#pragma once

#include <recorder.h>
#include <system_info.h>
#include <boost/json/fwd.hpp>
#include <ostream>
#include <span>
#include <string>

namespace Kbi {

// The interface of the different serializations of the recorded types. The
// declarations are written out explicitly (ES.30/ES.31 — no preprocessor
// metaprogramming); every derived class overrides the same five overloads.
class Serializer {
public:
    virtual ~Serializer() = default;

    // The streaming interface; every concrete serializer implements it.
    virtual void serialize(const UsbDeviceInfo& a, std::ostream& out) const = 0;
    virtual void serialize(const Device& a, std::ostream& out) const = 0;
    virtual void serialize(const Input& a, std::ostream& out) const = 0;
    virtual void serialize(const Recorder& a, std::ostream& out) const = 0;
    virtual void serialize(const SystemInfo& a, std::ostream& out) const = 0;

    // The convenience wrappers of the streaming interface; the base provides
    // them for free.
    virtual std::string serialize(const UsbDeviceInfo& a) const;
    virtual std::string serialize(const Device& a) const;
    virtual std::string serialize(const Input& a) const;
    virtual std::string serialize(const Recorder& a) const;
    virtual std::string serialize(const SystemInfo& a) const;

    virtual void serialize(const UsbDeviceInfo& a, std::span<char> out) const;
    virtual void serialize(const Device& a, std::span<char> out) const;
    virtual void serialize(const Input& a, std::span<char> out) const;
    virtual void serialize(const Recorder& a, std::span<char> out) const;
    virtual void serialize(const SystemInfo& a, std::span<char> out) const;
};

class JsonTextSerializer : public Serializer {
public:
    void serialize(const UsbDeviceInfo& a, std::ostream& out) const override;
    void serialize(const Device& a, std::ostream& out) const override;
    void serialize(const Input& a, std::ostream& out) const override;
    void serialize(const Recorder& a, std::ostream& out) const override;
    void serialize(const SystemInfo& a, std::ostream& out) const override;

    std::string serialize(const UsbDeviceInfo& a) const override;
    std::string serialize(const Device& a) const override;
    std::string serialize(const Input& a) const override;
    std::string serialize(const Recorder& a) const override;
    std::string serialize(const SystemInfo& a) const override;

    boost::json::value toJson(const UsbDeviceInfo& a) const;
    boost::json::value toJson(const Device& a) const;
    boost::json::value toJson(const Input& a) const;
    boost::json::value toJson(const Recorder& a) const;
    boost::json::value toJson(const SystemInfo& a) const;
};

class CborSerializer : public Serializer {
public:
    void serialize(const UsbDeviceInfo& a, std::ostream& out) const override;
    void serialize(const Device& a, std::ostream& out) const override;
    void serialize(const Input& a, std::ostream& out) const override;
    void serialize(const Recorder& a, std::ostream& out) const override;
    void serialize(const SystemInfo& a, std::ostream& out) const override;
};

} // namespace Kbi
