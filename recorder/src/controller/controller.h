#pragma once

#include "../serializer/serializer.h"
#include <recorder.h>
#include <atomic>
#include <boost/json/fwd.hpp>
#include <exception>
#include <format>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>
#include <mutex>
#include <spdlog/fwd.h>
#include <string>
#include <string_view>
#include <utility>

namespace Kbi {

class Controller {
public:
    Controller(Recorder& recorder, std::shared_ptr<spdlog::logger> logger)
        : m_recorder(recorder)
        , m_logger(std::move(logger))
    {
    }
    virtual ~Controller() = default;
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

    // Sends the already serialized value to every connected client (F.10 —
    // one named operation instead of three copied lambdas).
    void broadcastEvent(
        std::string_view type, const std::string& id, const std::string& json)
    {
        const auto payload
            = std::format(R"({{"type":"{}","id":"{}","data":{}}})", type, id, json);
        for (const auto& client : m_server.getClients())
            client->sendText(payload);
    }
};

class NeutralinoController : public Controller {
public:
    NeutralinoController(Recorder& recorder, std::shared_ptr<spdlog::logger> logger);
    void run() override;

private:
    ix::WebSocket m_ws;
    std::string m_accessToken;
    JsonTextSerializer m_serializer;

    void sendEvent(std::string_view event, const boost::json::value& payload);

    // Serializes the value and broadcasts it as an event; failures are
    // logged. One named operation instead of three copied lambdas (F.10).
    template<typename T>
    void sendSerialized(std::string_view event, const std::string& id, const T& value)
    {
        try {
            sendEvent(event, boost::json::object { { id, m_serializer.toJson(value) } });
        } catch (const std::exception& e) {
            m_logger->error("Neutralino event serialization failed: {}", e.what());
        }
    }
};

} // namespace Kbi
