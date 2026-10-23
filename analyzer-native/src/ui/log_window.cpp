#include "ui/log_window.h"

#include <imgui.h>
#include <spdlog/common.h>

#include <array>
#include <chrono>
#include <format>
#include <string>
#include <vector>

namespace Kbi::Ui {
namespace {

// The levels the filter offers, from chattiest to quietest.
constexpr std::array<spdlog::level::level_enum, 6> kFilterLevels = {
    spdlog::level::trace,
    spdlog::level::debug,
    spdlog::level::info,
    spdlog::level::warn,
    spdlog::level::err,
    spdlog::level::critical,
};

// A captured message made ready for drawing: the filter has been applied and
// the time stamp has been formatted.
struct Row {
    std::string prefix;
    spdlog::level::level_enum level = spdlog::level::info;
    std::string text;
};

ImVec4 levelColor(spdlog::level::level_enum level)
{
    switch (level) {
    case spdlog::level::trace:
    case spdlog::level::debug:
        return { 0.55f, 0.55f, 0.55f, 1.0f };
    case spdlog::level::critical:
        return { 1.0f, 0.25f, 0.9f, 1.0f };
    case spdlog::level::err:
        return { 1.0f, 0.45f, 0.45f, 1.0f };
    case spdlog::level::warn:
        return { 1.0f, 0.82f, 0.25f, 1.0f };
    default:
        return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }
}

// Lays out one line: the time, the level and the message that follows them.
void drawRow(const Row& row)
{
    ImGui::TextDisabled("%s", row.prefix.c_str());
    // The time stamps and the level names have a fixed width, so the columns
    // are simple offsets from the left edge.
    ImGui::SameLine(105.0f);
    ImGui::TextColored(levelColor(row.level), "%s", spdlog::level::to_string_view(row.level).data());
    ImGui::SameLine(160.0f);
    ImGui::TextUnformatted(row.text.c_str());
}

} // namespace

void LogWindow::draw(bool& open, LogStore& store)
{
    ImGui::SetNextWindowSize(ImVec2(760.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Logs", &open)) {
        ImGui::End();
        return;
    }

    // The snapshot is taken before the toolbar so that the copy button can
    // put the visible messages on the clipboard.
    std::vector<Row> rows;
    store.withEntries([&](const std::vector<LogStore::Entry>& entries) {
        rows.reserve(entries.size());
        for (const LogStore::Entry& entry : entries) {
            if (entry.level < m_minLevel)
                continue;
            Row row;
            row.prefix = std::format("{:%T}", std::chrono::floor<std::chrono::milliseconds>(entry.time));
            row.level = entry.level;
            row.text = entry.text;
            rows.push_back(std::move(row));
        }
    });

    if (ImGui::Button("Clear")) {
        store.clear();
        rows.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Copy")) {
        std::string out;
        for (const Row& row : rows)
            out += std::format("{} [{}] {}\n", row.prefix,
                spdlog::level::to_string_view(row.level), row.text);
        ImGui::SetClipboardText(out.c_str());
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Show from");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##min_level", spdlog::level::to_string_view(m_minLevel).data())) {
        for (const spdlog::level::level_enum level : kFilterLevels) {
            const bool selected = level == m_minLevel;
            if (ImGui::Selectable(spdlog::level::to_string_view(level).data(), selected))
                m_minLevel = level;
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);

    ImGui::Separator();

    ImGui::BeginChild("##log", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
        ImGuiWindowFlags_HorizontalScrollbar);
    // Only the rows that are on the screen are laid out, the buffer keeps
    // thousands of messages.
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()));
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            drawRow(rows[static_cast<std::size_t>(i)]);
    }
    clipper.End();

    if (m_autoScroll && !rows.empty() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    ImGui::End();
}

} // namespace Kbi::Ui
