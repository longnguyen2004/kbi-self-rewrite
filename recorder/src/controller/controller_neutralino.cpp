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

using namespace std::literals;
namespace json = boost::json;
namespace uuid = boost::uuids;

NeutralinoController::NeutralinoController(Recorder& recorder, std::shared_ptr<spdlog::logger> logger)
    : Controller(recorder, std::move(logger))
{
}

void NeutralinoController::sendNeutralinoEvent(
    std::string_view event, const json::value& payload)
{
    static uuid::random_generator gen;
    json::object msg = {
        { "id", uuid::to_string(gen()) },
        { "method", "app.broadcast" },
        { "accessToken", this->m_accessToken },
        { "data", { { "event", event }, { "data", payload } } }
    };
    auto msgStr = json::serialize(msg);
    m_logger->info("Sending to Neutralino: {}", msgStr);
    this->m_ws.sendText(msgStr);
}

void NeutralinoController::run()
{
    const auto& sysInfo = m_recorder.getSystemInfo();
    JsonTextSerializer serializer;
    auto conn1 = m_recorder.onUsbDevice().connect(
        [&](const std::string& id, const UsbDeviceInfo& device) {
            try {
                auto json = serializer.GetJson(device);
                this->sendNeutralinoEvent("usbDevice", json::object { { id, json } });
            } catch (const std::exception& e) {
                m_logger->error("dead: {}", e.what());
                return;
            }
        });
    auto conn2 = m_recorder.onDevice().connect(
        [&](const std::string& id, const Device& device) {
            try {
                auto json = serializer.GetJson(device);
                this->sendNeutralinoEvent("device", json::object { { id, json } });
            } catch (const std::exception& e) {
                m_logger->error("dead: {}", e.what());
                return;
            }
        });
    auto conn3 = m_recorder.onInput().connect(
        [&](const std::string& id, const Input& input) {
            try {
                auto json = serializer.GetJson(input);
                this->sendNeutralinoEvent("input", json::object { { id, json } });
            } catch (const std::exception& e) {
                m_logger->error("dead: {}", e.what());
                return;
            }
        });

    m_logger->info("Waiting for Neutralino connection info");
    std::error_code ec;
    auto connInfo = json::parse(std::cin).as_object();
    auto nlPort = connInfo.at("nlPort").as_string();
    auto nlToken = connInfo.at("nlToken").as_string();
    auto nlConnectToken = connInfo.at("nlConnectToken").as_string();
    auto nlExtensionId = connInfo.at("nlExtensionId").as_string();
    m_logger->info("Received Neutralino connection info: {}", json::serialize(connInfo));
    this->m_accessToken = nlToken;

    m_logger->info("Connecting to Neutralino server");
    this->m_ws.setUrl(
        std::format(
            "ws://localhost:{}?extensionId={}&connectToken={}",
            nlPort.c_str(), nlExtensionId.c_str(), nlConnectToken.c_str()));
    this->m_ws.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            json::object msgJson;
            try {
                msgJson = json::parse(msg->str).as_object();
            } catch (const std::exception&) {
                m_logger->warn("Can't parse: {}", msg->str);
                return;
            }
            m_logger->info("Received message from Neutralino: {}", msg->str);
            auto maybeEvent = msgJson.try_at("event");
            if (maybeEvent.has_error())
                return;
            auto event = maybeEvent.value().as_string();
            if (event == "start") {
                this->sendNeutralinoEvent("systemInfo", serializer.GetJson(sysInfo));
                this->m_recorder.start();
            } else if (event == "stop")
                this->m_recorder.stop();
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            m_logger->info("Connected to Neutralino server");
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            m_logger->error("WebSocket error: {}", msg->errorInfo.reason);
        }
    });

    this->m_ws.start();

    while (true) {
        // Just spin this thread
        std::this_thread::sleep_for(1s);
    }
}
