#include "controller.h"
#include "../system/info.h"
#include <boost/json.hpp>
#include <boost/uuid/random_generator.hpp>
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
        {"id", gen()},
        {"method", "app.broadcast"},
        {"access_token", this->m_access_token},
        {"data", {
            {"event", event},
            {"data", payload}
        }}
    };
    this->m_ws.sendText(json::serialize(msg));
}

void NeutralinoController::Run()
{
    JsonTextSerializer serializer;
    auto conn1 = m_recorder.OnUsbDevice().connect(
        [&](const std::string& id, const UsbDeviceInfo& device) {
            auto json = serializer.GetJson(device);
            this->_sendNeutralinoEvent("usbDevice", json);
        }
    );
    auto conn2 = m_recorder.OnDevice().connect(
        [&](const std::string& id, const Device& device) {
            auto json = serializer.GetJson(device);
            this->_sendNeutralinoEvent("device", json);
        }
    );
    auto conn3 = m_recorder.OnInput().connect(
        [&](const std::string& id, const Input& input) {
            auto json = serializer.GetJson(input);
            this->_sendNeutralinoEvent("input", json);
        }
    );

    m_logger->info("Waiting for Neutralino connection info");
    std::error_code ec;
    auto conn_info = json::parse(std::cin).as_object();
    auto nl_port = conn_info.at("nlPort").as_string();
    auto nl_token = conn_info.at("nlToken").as_string();
    auto nl_connect_token = conn_info.at("nlConnectToken").as_string();
    auto nl_extension_id = conn_info.at("nlExtensionId").as_string();

    m_logger->info("Connecting to Neutralino server");
    this->m_ws.setUrl(
        std::format(
            "ws://localhost:{}?extensionId={}&connectToken={}",
            nl_port, nl_extension_id, nl_connect_token
        )
    );
    this->m_ws.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            json::object msg_json;
            try {
                msg_json = json::parse(msg->str).as_object();
            }
            catch (const std::exception&) {
                return;
            }
            auto event = msg_json.at("event").as_string();
            if (event == "start")
                this->m_recorder.Start();
            else if (event == "stop")
                this->m_recorder.Stop();
        }
    });

    this->m_ws.start();

    auto sys_info = GetSystemInfo();
    this->_sendNeutralinoEvent("systemInfo", serializer.GetJson(sys_info));

    while (true) {
        // Just spin this thread
        std::this_thread::sleep_for(1s);
    }
}
