#include "../serializer/serializer.h"
#include <recorder.h>
#include <spdlog/fwd.h>
#include <uwebsockets/App.h>
#include <atomic>
#include <variant>

class Controller
{
public:
    Controller(Recorder& recorder, std::shared_ptr<spdlog::logger> logger):
        m_recorder(recorder), m_logger(logger) {};
    virtual void Run() = 0;

protected:
    Recorder& m_recorder;
    std::shared_ptr<spdlog::logger> m_logger;
};

class ConsoleController: public Controller
{
public:
    ConsoleController(Recorder& recorder, std::shared_ptr<spdlog::logger> logger);
    virtual void Run();
};

class WebSocketController: public Controller
{
public:
    WebSocketController(Recorder& recorder, std::shared_ptr<spdlog::logger> logger);
    virtual void Run();

private:
    struct SocketData
    {
    };
    uWS::App m_app;
    std::atomic<unsigned long> m_client_id = 0;

    JsonTextSerializer m_serializer;
};
