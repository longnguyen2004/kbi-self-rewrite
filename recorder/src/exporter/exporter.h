#pragma once

#include <recorder.h>
#include <ostream>

namespace Kbi {

// The interface of exporters that write a recording to a stream. It stays
// empty, the recorders are passed to the concrete exporters (I.25, C.121).
class Exporter {
public:
    virtual ~Exporter() = default;
    virtual void exportTo(std::ostream& out) const = 0;
};

// Exports the recording in the binary .kbi format.
class ExporterMatKbi : public Exporter {
public:
    explicit ExporterMatKbi(const Recorder& recorder);
    void exportTo(std::ostream& out) const override;

private:
    const Recorder& m_recorder;
};

} // namespace Kbi
