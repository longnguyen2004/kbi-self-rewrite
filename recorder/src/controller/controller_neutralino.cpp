#include "controller.h"
#include <system_info.h>
#include <boost/json.hpp>
#include <boost/uuid.hpp>
#include <chrono>
#include <exception>
#include <format>
#include <iostream>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <utility>

namespace Kbi {

using namespace std::literals;
namespace json = boost::json;
namespace uuid = boost::uuids;

NeutralinoController::NeutralinoController(
    Recorder& recorder, std::shared_ptr<spdlog::logger> logger)
    : m_ctx { .recorder = recorder, .logger = std::move(logger) }
{
}

void NeutralinoController::sendEvent(std::string_view event, const json::value& payload)
{
    static uuid::random_generator gen;
    json::object msg = { { "id", uuid::to_string(gen()) }, { "method", "app.broadcast" },
        { "accessToken", m_accessToken }, { "data", { { "event", event }, { "data", payload } } } };
    auto msgStr = json::serialize(msg);
    m_ctx.logger->info("Sending to Neutralino: {}", msgStr);
    m_ws.sendText(msgStr);
}

void NeutralinoController::run()
{
    auto conn1 = m_ctx.recorder.onUsbDevice().connect(
        [this](const std::string& id, const UsbDeviceInfo& device) {
            sendSerialized("usbDevice", id, device);
        });
    auto conn2
        = m_ctx.recorder.onDevice().connect([this](const std::string& id, const Device& device) {
              sendSerialized("device", id, device);
          });
    auto conn3 = m_ctx.recorder.onInput().connect(
        [this](const std::string& id, const Input& input) { sendSerialized("input", id, input); });

    m_ctx.logger->info("Waiting for Neutralino connection info");
    const auto connInfo = json::parse(std::cin).as_object();
    const auto nlPort = connInfo.at("nlPort").as_string();
    const auto nlToken = connInfo.at("nlToken").as_string();
    const auto nlConnectToken = connInfo.at("nlConnectToken").as_string();
    const auto nlExtensionId = connInfo.at("nlExtensionId").as_string();
    m_ctx.logger->info("Received Neutralino connection info: {}", json::serialize(connInfo));
    m_accessToken = nlToken;

    m_ctx.logger->info("Connecting to Neutralino server");
    m_ws.setUrl(std::format("ws://localhost:{}?extensionId={}&connectToken={}", nlPort.c_str(),
        nlExtensionId.c_str(), nlConnectToken.c_str()));
    m_ws.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            json::object msgJson;
            try {
                msgJson = json::parse(msg->str).as_object();
            } catch (const std::exception&) {
                m_ctx.logger->warn("Can't parse: {}", msg->str);
                return;
            }
            m_ctx.logger->info("Received message from Neutralino: {}", msg->str);
            auto maybeEvent = msgJson.try_at("event");
            if (maybeEvent.has_error())
                return;
            auto event = maybeEvent.value().as_string();
            if (event == "start") {
                sendEvent("systemInfo", m_serializer.toJson(m_ctx.recorder.systemInfo()));
                m_ctx.recorder.start();
            } else if (event == "stop")
                m_ctx.recorder.stop();
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            m_ctx.logger->info("Connected to Neutralino server");
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            m_ctx.logger->error("WebSocket error: {}", msg->errorInfo.reason);
        }
    });

    m_ws.start();

    while (true) {
        // Just spin this thread
        std::this_thread::sleep_for(1s);
    }
}

} // namespace Kbi
