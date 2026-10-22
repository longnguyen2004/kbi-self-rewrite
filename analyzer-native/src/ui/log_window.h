#pragma once

#include "ui/log_store.h"

namespace Kbi::Ui {

// The "Logs" dialog, it shows what spdlog has written to the LogStore sink.
class LogWindow {
public:
    void draw(bool* open, LogStore& store);

private:
    spdlog::level::level_enum m_minLevel = spdlog::level::info;
    bool m_autoScroll = true;
};

} // namespace Kbi::Ui
