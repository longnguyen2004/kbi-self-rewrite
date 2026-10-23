#pragma once

#include <recorder.h>
#include <ostream>

namespace Kbi {

// Base class for exporters that write the recording of the `Recorder` to a
// stream.
class Exporter {
public:
    explicit Exporter(const Recorder& recorder)
        : m_recorder(recorder)
    {
    }
    virtual void exportTo(std::ostream& out) = 0;

protected:
    const Recorder& m_recorder;
};

// Exports the recording in the binary .kbi format.
class ExporterMatKbi : public Exporter {
public:
    explicit ExporterMatKbi(const Recorder& recorder)
        : Exporter(recorder)
    {
    }
    void exportTo(std::ostream& out) override;
};

} // namespace Kbi
