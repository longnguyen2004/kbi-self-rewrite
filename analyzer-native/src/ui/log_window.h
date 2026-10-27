#pragma once

#include <ui/log_store.h>

#include <string>
#include <vector>

namespace Kbi::Ui {

// The "Logs" dialog, it shows what spdlog has written to the LogStore sink.
class LogWindow {
public:
    void draw(bool& open, LogStore& store);

private:
    spdlog::level::level_enum m_minLevel = spdlog::level::info;
    bool m_autoScroll = true;

    // The rows of the current frame; the buffer and its strings keep their
    // capacity across frames (Per.15).
    struct Row {
        std::string prefix;
        spdlog::level::level_enum level = spdlog::level::info;
        std::string text;
    };
    static void drawRow(const Row& row);
    std::vector<Row> m_rows;
};

} // namespace Kbi::Ui
