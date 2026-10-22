#pragma once

#include <recorder.h>
#include <ostream>

class Exporter {
public:
    explicit Exporter(const Recorder& recorder)
        : m_recorder(recorder) { };
    virtual void exportTo(std::ostream& out) = 0;

protected:
    const Recorder& m_recorder;
};

class ExporterMatKbi : public Exporter {
public:
    explicit ExporterMatKbi(const Recorder& recorder)
        : Exporter(recorder) { };
    void exportTo(std::ostream& out) override;
};
