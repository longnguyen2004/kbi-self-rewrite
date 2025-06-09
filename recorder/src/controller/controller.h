#include <recorder.h>
#include <spdlog/fwd.h>

class Controller
{
public:
    Controller(Recorder& recorder, spdlog::logger& logger): m_recorder(recorder), m_logger(logger) {};
    virtual void Run() = 0;

protected:
    Recorder& m_recorder;
    spdlog::logger& m_logger;
};

class ConsoleController: public Controller
{
public:
    ConsoleController(Recorder& recorder, spdlog::logger& logger);
    virtual void Run();
};
