#pragma once

#include <recorder.h>
#include <ostream>

class Exporter
{
public:
    Exporter(const Recorder& recorder): m_recorder(recorder) {};
    virtual void Export(std::ostream& out) = 0;

protected:
    const Recorder& m_recorder;
};

class Exporter_MatKbi: public Exporter
{
public:
    Exporter_MatKbi(const Recorder& recorder): Exporter(recorder) {};
    virtual void Export(std::ostream& out);
};

