#include "controller/controller.h"
#include <recorder.h>
#include <boost/di.hpp>
#include <boost/program_options.hpp>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cassert>
#include <cstdio>
#include <exception>
#include <iostream>
#include <istream>
#include <iterator>
#include <print>

#ifdef __linux__
#include <sys/stat.h>
#endif

namespace di = boost::di;
namespace po = boost::program_options;

enum class ProgramMode {
    CONSOLE,
    WEBSOCKET,
    NEUTRALINO
};

std::istream& operator>>(std::istream& in, ProgramMode& mode)
{
    std::string token;
    in >> token;

    if (token == "console") {
        mode = ProgramMode::CONSOLE;
    } else if (token == "websocket") {
        mode = ProgramMode::WEBSOCKET;
    } else if (token == "neutralino") {
        mode = ProgramMode::NEUTRALINO;
    } else {
        in.setstate(std::ios_base::failbit);
    }
    return in;
}

int main(int argc, char const* argv[])
{
    std::string logPath;
    ProgramMode mode;
    po::options_description desc("Allowed options");
    desc.add_options()("help", "produce help message")(
        "mode",
        po::value<ProgramMode>(&mode)->default_value(ProgramMode::CONSOLE, "console"),
        "Operating mode (console, websocket, neutralino)")(
        "log-path",
        po::value<std::string>(&logPath)->default_value("log.txt"),
        "Path for the log file");
    po::variables_map vm;

    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    } catch (const std::exception& e) {
        std::println("Error: {}", e.what());
        return 1;
    }
    if (vm.contains("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    const auto injector = boost::di::make_injector(
        boost::di::bind<spdlog::logger>.to([&] -> std::shared_ptr<spdlog::logger> {
            auto consoleSink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
            consoleSink->set_level(spdlog::level::info);

            static spdlog::file_event_handlers handlers;
#ifdef __linux__
            handlers.after_open = [](const spdlog::filename_t& filename, std::FILE* file) {
                int fd = fileno(file);
                fchmod(fd, 0666);
            };
#endif
            auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logPath, true, handlers);
            fileSink->set_level(spdlog::level::trace);

            const std::array<std::shared_ptr<spdlog::sinks::sink>, 2> sinks = { consoleSink, fileSink };
            auto multiSink = std::make_shared<spdlog::logger>("", std::begin(sinks), std::end(sinks));
            multiSink->set_level(spdlog::level::trace);

            return multiSink;
        }),
        boost::di::bind<Controller>.to([&](const auto& injector) -> Controller& {
            switch (mode) {
            case ProgramMode::CONSOLE:
                return injector.template create<ConsoleController&>();
            case ProgramMode::WEBSOCKET:
                return injector.template create<WebSocketController&>();
            case ProgramMode::NEUTRALINO:
                return injector.template create<NeutralinoController&>();
            default:
                throw std::runtime_error("Unknown/unimplemented program mode");
            }
        }));
    auto logger = injector.create<std::shared_ptr<spdlog::logger>>();
    logger->info("Saving log to {}", logPath);
    try {
        auto& controller = injector.create<Controller&>();
        controller.run();
    } catch (const std::exception& e) {
        logger->error("An exception occurred: {}", e.what());
    }
    return 0;
}
