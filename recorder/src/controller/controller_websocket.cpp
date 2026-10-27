#include "controller.h"
#include <boost/url/parse.hpp>
#include <boost/url/url_view.hpp>
#include <charconv>
#include <format>
#include <spdlog/spdlog.h>
#include <string>
#include <utility>

namespace Kbi {
namespace {

// The port the WebSocket controller listens on.
constexpr unsigned long kWebSocketPort = 9001;

std::optional<unsigned long> parseIdFromUri(const std::string& uri)
{
    const auto result = boost::urls::parse_origin_form(uri);
    if (!result)
        return std::nullopt;
    const auto params = result->params();
    const auto it = params.find("id");
    if (it == params.end())
        return std::nullopt;
    if (!(*it).has_value)
        return std::nullopt;
    const std::string& value = (*it).value;
    unsigned long idNum;
    const auto res = std::from_chars(value.data(), value.data() + value.size(), idNum);
    if (res.ec != std::errc {} || res.ptr != value.data() + value.size())
        return std::nullopt;
    return idNum;
}

} // namespace

WebSocketController::WebSocketController(Recorder& recorder, std::shared_ptr<spdlog::logger> logger)
    : m_ctx { .recorder = recorder, .logger = std::move(logger) }
    , m_server(kWebSocketPort, "0.0.0.0")
{
    m_server.setOnClientMessageCallback(
        [this](const std::shared_ptr<ix::ConnectionState>& connectionState,
            ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Open) {
                const auto uri = msg->openInfo.uri;
                const auto idOpt = parseIdFromUri(uri);
                if (!idOpt.has_value()) {
                    m_ctx.logger->warn(
                        "WebSocket connection rejected: missing or invalid id in uri '{}'", uri);
                    webSocket.close(ix::WebSocketCloseConstants::kProtocolErrorCode,
                        "ID must be present and be a positive number");
                    return;
                }
                const unsigned long idNum = *idOpt;
                if (idNum == 0) {
                    m_ctx.logger->warn("WebSocket connection rejected: id must be positive");
                    webSocket.close(
                        ix::WebSocketCloseConstants::kProtocolErrorCode, "ID must be positive");
                    return;
                }
                unsigned long expected = 0;
                if (!m_clientId.compare_exchange_strong(expected, idNum)) {
                    m_ctx.logger->warn("WebSocket connection rejected: only one client allowed");
                    webSocket.close(ix::WebSocketCloseConstants::kProtocolErrorCode,
                        "Only one client can be connected at a time");
                    return;
                }
                {
                    std::scoped_lock lock(m_ownerMutex);
                    m_owner = &webSocket;
                }
                m_ctx.logger->info("WebSocket client connected with id {}", idNum);
            } else if (msg->type == ix::WebSocketMessageType::Close) {
                m_ctx.logger->info("WebSocket client disconnected");
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
                    m_ctx.recorder.start();
                else if (message == "stop")
                    m_ctx.recorder.stop();
            }
        });
}

void WebSocketController::run()
{
    const auto res = m_server.listen();
    if (!res.first) {
        m_ctx.logger->error("WebSocket server failed to listen: {}", res.second);
        return;
    }
    m_server.start();
    m_ctx.logger->info("WebSocket server listening on port {}", kWebSocketPort);

    auto conn1 = m_ctx.recorder.onUsbDevice().connect(
        [this](const std::string& id, const UsbDeviceInfo& device) {
            broadcastEvent("usb_device", id, m_serializer.serialize(device));
        });
    auto conn2
        = m_ctx.recorder.onDevice().connect([this](const std::string& id, const Device& device) {
              broadcastEvent("device", id, m_serializer.serialize(device));
          });
    auto conn3
        = m_ctx.recorder.onInput().connect([this](const std::string& id, const Input& input) {
              broadcastEvent("input", id, m_serializer.serialize(input));
          });

    m_server.wait();

    conn1.disconnect();
    conn2.disconnect();
    conn3.disconnect();
}

} // namespace Kbi
