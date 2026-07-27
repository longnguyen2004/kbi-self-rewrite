#include <recorder.h>
#include "controller/controller.h"
#include <boost/di.hpp>
#include <boost/program_options.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <iterator>
#include <stdio.h>

#ifdef __linux__
#include <sys/stat.h>
#endif

namespace di = boost::di;
namespace po = boost::program_options;

int main(int argc, char const *argv[])
{
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("websocket", "run in websocket mode");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    const auto injector = di::make_injector(
        di::bind<spdlog::logger>.to([]() -> std::shared_ptr<spdlog::logger> {
            auto console_sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
            console_sink->set_level(spdlog::level::info);

            static spdlog::file_event_handlers handlers;
#ifdef __linux__
            handlers.after_open = [](spdlog::filename_t filename, std::FILE* file) {
                int fd = fileno(file);
                fchmod(fd, 0666);
            };
#endif
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt", true, handlers);
            file_sink->set_level(spdlog::level::trace);

            std::shared_ptr<spdlog::sinks::sink> sinks[] = {console_sink, file_sink};
            auto multi_sink = std::make_shared<spdlog::logger>("", std::begin(sinks), std::end(sinks));
            multi_sink->set_level(spdlog::level::trace);

            return multi_sink;
        }),
        di::bind<Controller>.to([&](const auto& injector) -> Controller& {
            if (vm.count("websocket"))
                return injector.template create<WebSocketController&>();
            return injector.template create<ConsoleController&>();
        })
    );
    auto& controller = injector.create<Controller&>();
    controller.Run();
    return 0;
}
