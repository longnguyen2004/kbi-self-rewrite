#pragma once

#include <spdlog/common.h>
#include <spdlog/sinks/base_sink.h>

#include <chrono>
#include <concepts>
#include <cstddef>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Kbi::Ui {

// The spdlog sink that captures the log messages for the "Logs" dialog. The
// recorder logs into the default logger, so everything it reports is captured
// as well. The messages are kept in a bounded buffer, the oldest ones are
// dropped when it fills up.
class LogStore : public spdlog::sinks::base_sink<std::mutex> {
public:
    struct Entry {
        std::chrono::system_clock::time_point time;
        spdlog::level::level_enum level = spdlog::level::info;
        std::string text;
    };

    // The number of messages that are kept.
    [[nodiscard]] std::size_t capacity() const { return m_capacity; }
    void setCapacity(std::size_t capacity) { m_capacity = capacity; }

    void clear()
    {
        std::scoped_lock lock(mutex_);
        m_entries.clear();
    }

    // The captured entries, oldest first, visited while the sink's lock is
    // held so that recorder threads cannot add messages in the middle.
    template<typename Visitor>
        requires std::invocable<Visitor, std::vector<Entry>&>
    void withEntries(Visitor&& visitor)
    {
        std::scoped_lock lock(mutex_);
        visitor(m_entries);
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        Entry entry;
        entry.time = msg.time;
        entry.level = msg.level;
        entry.text.assign(msg.payload.data(), msg.payload.size());

        m_entries.push_back(std::move(entry));
        if (m_entries.size() > m_capacity)
            m_entries.erase(
                m_entries.begin(), m_entries.end() - static_cast<std::ptrdiff_t>(m_capacity));
    }

    void flush_() override { }

private:
    std::size_t m_capacity = 5000;
    std::vector<Entry> m_entries;
};

} // namespace Kbi::Ui
