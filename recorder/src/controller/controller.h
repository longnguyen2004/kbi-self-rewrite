#include "../serializer/serializer.h"
#include <recorder.h>
#include <atomic>
#include <boost/json/fwd.hpp>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <mutex>
#include <spdlog/fwd.h>
#include <string_view>
#include <utility>

class Controller {
public:
    Controller(Recorder& recorder, std::shared_ptr<spdlog::logger> logger)
        : m_recorder(recorder)
        , m_logger(std::move(logger)) { };
    virtual void run() = 0;

protected:
    Recorder& m_recorder;
    std::shared_ptr<spdlog::logger> m_logger;
};

class ConsoleController : public Controller {
public:
    ConsoleController(Recorder& recorder, std::shared_ptr<spdlog::logger> logger);
    void run() override;
};

class WebSocketController : public Controller {
public:
    WebSocketController(Recorder& recorder, std::shared_ptr<spdlog::logger> logger);
    void run() override;

private:
    ix::WebSocketServer m_server;
    std::atomic<unsigned long> m_clientId = 0;
    std::mutex m_ownerMutex;
    ix::WebSocket* m_owner = nullptr;

    JsonTextSerializer m_serializer;
};

class NeutralinoController : public Controller {
public:
    NeutralinoController(Recorder& recorder, std::shared_ptr<spdlog::logger> logger);
    void run() override;

private:
    ix::WebSocket m_ws;
    std::string m_accessToken;

    void sendNeutralinoEvent(std::string_view event, const boost::json::value& payload);
};
