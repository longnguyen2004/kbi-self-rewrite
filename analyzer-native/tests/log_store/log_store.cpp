#include <boost/ut.hpp>

#include <memory>
#include <print>
#include <spdlog/spdlog.h>
#include <string>
#include <ui/log_store.h>
#include <vector>

int main()
{
    using namespace boost::ut;

    "log store"_test = [] {
        const auto store = std::make_shared<Kbi::Ui::LogStore>();
        store->setCapacity(3);

        const auto logger = std::make_unique<spdlog::logger>("test", store);
        logger->info("hello {}", 42);
        logger->warn("watch out");
        logger->error("oops");

        std::vector<std::string> texts;
        std::vector<spdlog::level::level_enum> levels;
        store->withEntries([&](const std::vector<Kbi::Ui::LogStore::Entry>& entries) {
            expect(entries.size() == 3) << "all messages are captured";
            for (const Kbi::Ui::LogStore::Entry& entry : entries) {
                texts.push_back(entry.text);
                levels.push_back(entry.level);
            }
        });
        expect(texts.size() == 3 && texts[0] == "hello 42" && texts[2] == "oops")
            << "the messages keep the order they arrived in";
        expect(levels.size() == 3 && levels[1] == spdlog::level::warn)
            << "the message levels are preserved";

        // A debug message is not logged by the default level, but it is once it is
        // enabled.
        logger->debug("quiet one");
        std::size_t count = 0;
        store->withEntries(
            [&](const std::vector<Kbi::Ui::LogStore::Entry>& entries) { count = entries.size(); });
        expect(count == 3) << "debug is not logged at the default level";
        logger->set_level(spdlog::level::debug);
        logger->debug("quiet one");
        // The buffer could hold 4 messages but is bounded to 3, so the first info
        // message is dropped by the new one.
        std::string first;
        store->withEntries([&](const std::vector<Kbi::Ui::LogStore::Entry>& entries) {
            count = entries.size();
            if (!entries.empty())
                first = entries.front().text;
        });
        expect(count == 3 && first == "watch out") << "debug is logged once its level is enabled";

        // The buffer is bounded, the oldest messages are dropped.
        for (int i = 0; i < 5; ++i)
            logger->info("drop {}", i);
        store->withEntries([&](const std::vector<Kbi::Ui::LogStore::Entry>& entries) {
            count = entries.size();
            if (!entries.empty())
                first = entries.front().text;
        });
        expect(count == 3) << "the buffer keeps the last messages";
        expect(first == "drop 2") << "the oldest messages are dropped first";

        logger->flush();
        store->clear();
        store->withEntries([&](const std::vector<Kbi::Ui::LogStore::Entry>& entries) {
            expect(entries.empty()) << "clearing removes every message";
        });
    };
}
