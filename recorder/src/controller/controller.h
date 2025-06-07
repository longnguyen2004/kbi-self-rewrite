#include <recorder.h>

class Controller
{
public:
    Controller(Recorder& recorder): m_recorder(recorder) {};
    virtual void Run() = 0;

protected:
    Recorder& m_recorder;
};

class ConsoleController: public Controller
{
public:
    ConsoleController(Recorder& recorder);
    virtual void Run();
};
