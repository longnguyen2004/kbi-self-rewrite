#include "controller.h"
#include "../system/info.h"
#include <boost/json.hpp>
#include <boost/uuid.hpp>
#include <chrono>
#include <exception>
#include <format>
#include <iostream>
#include <string>
#include <spdlog/spdlog.h>
#include <thread>

using namespace std::literals;
namespace json = boost::json;
namespace uuid = boost::uuids;

NeutralinoController::NeutralinoController(Recorder& recorder, std::shared_ptr<spdlog::logger> logger):
    Controller(recorder, logger), m_ws()
{
}

void NeutralinoController::_sendNeutralinoEvent(
    std::string_view event, const json::value& payload
) {
    static uuid::random_generator gen;
    json::object msg = {
        {"id", uuid::to_string(gen())},
        {"method", "app.broadcast"},
        {"accessToken", this->m_access_token},
        {"data", {
            {"event", event},
            {"data", payload}
        }}
    };
    auto msg_str = json::serialize(msg);
    m_logger->info("Sending to Neutralino: {}", msg_str);
    this->m_ws.sendText(msg_str);
}

void NeutralinoController::Run()
{
    auto sys_info = GetSystemInfo();
    JsonTextSerializer serializer;
    auto conn1 = m_recorder.OnUsbDevice().connect(
        [&](const std::string& id, const UsbDeviceInfo& device) {
            try {
                auto json = serializer.GetJson(device);
                this->_sendNeutralinoEvent("usbDevice", json::object{{id, json}});
            }
            catch (const std::exception& e) {
                m_logger->error("dead: {}", e.what());
                return;
            }
        }
    );
    auto conn2 = m_recorder.OnDevice().connect(
        [&](const std::string& id, const Device& device) {
            try {
                auto json = serializer.GetJson(device);
                this->_sendNeutralinoEvent("device", json::object{{id, json}});
            }
            catch (const std::exception& e) {
                m_logger->error("dead: {}", e.what());
                return;
            }
        }
    );
    auto conn3 = m_recorder.OnInput().connect(
        [&](const std::string& id, const Input& input) {
            try {
                auto json = serializer.GetJson(input);
                this->_sendNeutralinoEvent("input", json::object{{id, json}});
            }
            catch (const std::exception& e) {
                m_logger->error("dead: {}", e.what());
                return;
            }
        }
    );

    m_logger->info("Waiting for Neutralino connection info");
    std::error_code ec;
    auto conn_info = json::parse(std::cin).as_object();
    auto nl_port = conn_info.at("nlPort").as_string();
    auto nl_token = conn_info.at("nlToken").as_string();
    auto nl_connect_token = conn_info.at("nlConnectToken").as_string();
    auto nl_extension_id = conn_info.at("nlExtensionId").as_string();
    m_logger->info("Received Neutralino connection info: {}", json::serialize(conn_info));
    this->m_access_token = nl_token;

    m_logger->info("Connecting to Neutralino server");
    this->m_ws.setUrl(
        std::format(
            "ws://localhost:{}?extensionId={}&connectToken={}",
            nl_port.c_str(), nl_extension_id.c_str(), nl_connect_token.c_str()
        )
    );
    this->m_ws.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            json::object msg_json;
            try {
                msg_json = json::parse(msg->str).as_object();
            }
            catch (const std::exception&) {
                m_logger->warn("Can't parse: {}", msg->str);
                return;
            }
            m_logger->info("Received message from Neutralino: {}", msg->str);
            auto maybe_event = msg_json.try_at("event");
            if (maybe_event.has_error())
                return;
            auto event = maybe_event.value().as_string();
            if (event == "start")
            {
                this->_sendNeutralinoEvent("systemInfo", serializer.GetJson(sys_info));
                this->m_recorder.Start();
            }
            else if (event == "stop")
                this->m_recorder.Stop();
        }
        else if (msg->type == ix::WebSocketMessageType::Open) {
            m_logger->info("Connected to Neutralino server");
        }
        else if (msg->type == ix::WebSocketMessageType::Error) {
            m_logger->error("WebSocket error: {}", msg->errorInfo.reason);
        }
    });

    this->m_ws.start();

    while (true) {
        // Just spin this thread
        std::this_thread::sleep_for(1s);
    }
}
