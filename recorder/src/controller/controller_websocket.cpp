#include "controller.h"
#include <boost/url/parse.hpp>
#include <boost/url/url_view.hpp>
#include <charconv>
#include <format>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

namespace {
std::optional<unsigned long> parseIdFromUri(const std::string& uri)
{
    auto result = boost::urls::parse_origin_form(uri);
    if (!result)
        return std::nullopt;
    auto params = result->params();
    auto it = params.find("id");
    if (it == params.end())
        return std::nullopt;
    if (!(*it).has_value)
        return std::nullopt;
    const std::string& value = (*it).value;
    unsigned long idNum;
    auto res = std::from_chars(value.data(), value.data() + value.size(), idNum);
    if (res.ec != std::errc {} || res.ptr != value.data() + value.size())
        return std::nullopt;
    return idNum;
}

void broadcast(ix::WebSocketServer& server, const std::string& message)
{
    auto clients = server.getClients();
    for (const auto& client : clients) {
        client->sendText(message);
    }
}
}

WebSocketController::WebSocketController(Recorder& recorder, std::shared_ptr<spdlog::logger> logger)
    : Controller(recorder, std::move(logger))
    , m_server(9001, "0.0.0.0")
{
    m_server.setOnClientMessageCallback(
        [this](const std::shared_ptr<ix::ConnectionState>& connectionState,
            ix::WebSocket& webSocket,
            const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open) {
                auto uri = msg->openInfo.uri;
                auto idOpt = parseIdFromUri(uri);
                if (!idOpt.has_value()) {
                    m_logger->warn("WebSocket connection rejected: missing or invalid id in uri '{}'", uri);
                    webSocket.close(ix::WebSocketCloseConstants::kProtocolErrorCode, "ID must be present and be a positive number");
                    return;
                }
                unsigned long idNum = *idOpt;
                if (idNum == 0) {
                    m_logger->warn("WebSocket connection rejected: id must be positive");
                    webSocket.close(ix::WebSocketCloseConstants::kProtocolErrorCode, "ID must be positive");
                    return;
                }
                unsigned long expected = 0;
                if (!m_clientId.compare_exchange_strong(expected, idNum)) {
                    m_logger->warn("WebSocket connection rejected: only one client allowed");
                    webSocket.close(ix::WebSocketCloseConstants::kProtocolErrorCode, "Only one client can be connected at a time");
                    return;
                }
                {
                    std::scoped_lock lock(m_ownerMutex);
                    m_owner = &webSocket;
                }
                m_logger->info("WebSocket client connected with id {}", idNum);
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                m_logger->info("WebSocket client disconnected");
                std::scoped_lock lock(m_ownerMutex);
                if (m_owner == &webSocket) {
                    m_clientId = 0;
                    m_owner = nullptr;
                }
            } else if (msg->type == ix::WebSocketMessageType::Message) {
                if (msg->binary)
                    return;
                const auto& message = msg->str;
                if (message == "start")
                    m_recorder.start();
                else if (message == "stop")
                    m_recorder.stop();
            }
        });
}

void WebSocketController::run()
{
    auto res = m_server.listen();
    if (!res.first) {
        m_logger->error("WebSocket server failed to listen: {}", res.second);
        return;
    }
    m_server.start();
    m_logger->info("WebSocket server listening on port 9001");

    auto conn1 = m_recorder.onUsbDevice().connect(
        [this](const std::string& id, const UsbDeviceInfo& device) {
            auto json = m_serializer.Serialize(device);
            auto payload = std::format(R"({{"type":"usb_device","id":"{}","data":{}}})", id, json);
            broadcast(m_server, payload);
        });
    auto conn2 = m_recorder.onDevice().connect(
        [this](const std::string& id, const Device& device) {
            auto json = m_serializer.Serialize(device);
            auto payload = std::format(R"({{"type":"device","id":"{}","data":{}}})", id, json);
            broadcast(m_server, payload);
        });
    auto conn3 = m_recorder.onInput().connect(
        [this](const std::string& id, const Input& input) {
            auto json = m_serializer.Serialize(input);
            auto payload = std::format(R"({{"type":"input","id":"{}","data":{}}})", id, json);
            broadcast(m_server, payload);
        });

    m_server.wait();

    conn1.disconnect();
    conn2.disconnect();
    conn3.disconnect();
}
