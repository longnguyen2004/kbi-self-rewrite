#include "controller.h"
#include <charconv>
#include <format>
#include <string>
#include <spdlog/spdlog.h>

WebSocketController::WebSocketController(Recorder& recorder, std::shared_ptr<spdlog::logger> logger):
    Controller(recorder, logger)
{
    m_app.ws<SocketData>("/", {
        .idleTimeout = 10,
        .upgrade = [this](auto* res, auto* req, auto* ctx) {
            auto id = req->getQuery("id");
            unsigned long id_num;
            if (std::from_chars(id.data(), id.data() + id.size(), id_num).ec != std::errc{})
            {
                res->writeStatus("400 Bad Request")->end("ID must be present and be a positive number");
                return;
            }
            if (id_num == 0)
            {
                res->writeStatus("400 Bad Request")->end("ID must be positive");
                return;
            }
            unsigned long expected = 0;
            if (!m_client_id.compare_exchange_strong(expected, id_num))
            {
                res->writeStatus("400 Bad Request")->end("Only one client can be connected at a time");
                return;
            }
            res->template upgrade<SocketData>(
                {},
                req->getHeader("sec-websocket-key"),
                req->getHeader("sec-websocket-protocol"),
                req->getHeader("sec-websocket-extensions"),
                ctx
            );
        },
        .open = [this](auto* ws) {
            ws->subscribe("data");
        },
        .message = [this](auto* ws, std::string_view message, uWS::OpCode code) {
            if (code != uWS::OpCode::TEXT)
                return;
            if (message == "start")
                m_recorder.Start();
            else if (message == "stop")
                m_recorder.Stop();
        },
        .close = [this](auto* ws, int, std::string_view) {
            m_client_id = 0;
        }
    }).listen(9001, [this](auto*) {
        m_logger->info("WebSocket server listening on port 9001");
    });
}

void WebSocketController::Run()
{
    auto loop = uWS::Loop::get();
    auto conn1 = m_recorder.OnUsbDevice().connect(
        [this, loop](const std::string& id, const UsbDeviceInfo& device) {
            auto json = m_serializer.Serialize(device);
            loop->defer([this, id, json = std::move(json)]() {
                m_app.publish(
                    "data", std::format(R"({{"type":"usb_device","id":"{}","data":{}}})", id, json),
                    uWS::OpCode::TEXT, true
                );
            });
        }
    );
    auto conn2 = m_recorder.OnDevice().connect(
        [this, loop](const std::string& id, const Device& device) {
            auto json = m_serializer.Serialize(device);
            loop->defer([this, id, json = std::move(json)]() {
                m_app.publish(
                    "data", std::format(R"({{"type":"device","id":"{}","data":{}}})", id, json),
                    uWS::OpCode::TEXT, true
                );
            });
        }
    );
    auto conn3 = m_recorder.OnInput().connect(
        [this, loop](const std::string& id, const Input& input) {
            auto json = m_serializer.Serialize(input);
            loop->defer([this, id, json = std::move(json)]() {
                m_app.publish(
                    "data", std::format(R"({{"type":"input","id":"{}","data":{}}})", id, json),
                    uWS::OpCode::TEXT, true
                );
            });
        }
    );
    m_app.run();
    conn1.disconnect();
    conn2.disconnect();
    conn3.disconnect();
}
