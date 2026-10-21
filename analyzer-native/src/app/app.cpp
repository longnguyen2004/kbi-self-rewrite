#include "app/app.h"

#include "app/theme.h"
#include "parser/parser.h"

#include <chrono>
#include <datetime.h>
#include "ui/info_window.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <format>
#include <chrono>
#include <cstdio>

namespace kbi::app {
namespace {

constexpr int kBinRates[] = {1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000};

// Piecewise linear interpolation between the event count colors.
ImVec4 event_count_color(double value) {
    static const double stops[] = {50.0, 200.0, 600.0, 1500.0};
    static const ImVec4 colors[] = {
        ImVec4(238.0f / 255.0f, 0.0f, 0.0f, 1.0f),
        ImVec4(234.0f / 255.0f, 119.0f / 255.0f, 0.0f, 1.0f),
        ImVec4(221.0f / 255.0f, 221.0f / 255.0f, 0.0f, 1.0f),
        ImVec4(0.0f, 238.0f / 255.0f, 0.0f, 1.0f),
    };
    if (value <= stops[0])
        return colors[0];
    if (value >= stops[3])
        return colors[3];
    for (int i = 0; i < 3; ++i) {
        if (value > stops[i] && value <= stops[i + 1]) {
            const double t = (value - stops[i]) / (stops[i + 1] - stops[i]);
            return ImVec4(
                colors[i].x + static_cast<float>(t) * (colors[i + 1].x - colors[i].x),
                colors[i].y + static_cast<float>(t) * (colors[i + 1].y - colors[i].y),
                colors[i].z + static_cast<float>(t) * (colors[i + 1].z - colors[i].z), 1.0f);
        }
    }
    return colors[0];
}

} // namespace

App::App() {
    reset_analysis();
}

void App::reset_analysis() {
    m_analyzer.reset();
    m_timeline.reset();
    m_result = Result{};
    m_has_result = false;
    m_device_ids.clear();
    m_diff_sync = ui::ChartSync{};
    m_freq_sync = ui::ChartSync{};
    m_charts.reset_zoom();
}

// The directory the file dialog opens in.
std::string App::start_directory() const {
    if (m_last_path.empty())
        return ".";
    const std::filesystem::path parent = std::filesystem::path(m_last_path).parent_path();
    if (parent.empty())
        return ".";
    return parent.string();
}

void App::update_device_ids() {
    m_device_ids.clear();
    m_device_ids.reserve(m_result.devices.size());
    for (const auto& [id, device] : m_result.devices)
        m_device_ids.push_back(id);
}

void App::ingest(const Result& result) {
    m_result = result;
    m_has_result = true;

    std::vector<double> timestamps;
    for (const auto& [id, events] : m_result.inputs) {
        for (const InputEvent& event : events)
            timestamps.push_back(event.timestamp);
    }
    std::sort(timestamps.begin(), timestamps.end());
    m_analyzer.add(timestamps);

    for (const auto& [id, events] : m_result.inputs) {
        for (const InputEvent& event : events)
            m_timeline.add(id, event);
    }

    update_device_ids();
}

void App::open_file(const std::string& path) {
    const auto parsed = parse_result_file(path);
    if (!parsed) {
        m_status = std::format("Could not read {}", path);
        return;
    }
    reset_analysis();
    ingest(*parsed);
    // The timeline starts at the beginning of the recording.
    m_charts.set_timeline_range(0.0, 10.0);
    m_last_path = path;
    m_status = std::format("Loaded {}", path);
}

bool App::load(const std::string& path) {
    open_file(path);
    return m_has_result;
}

bool App::start_recording() {
    reset_analysis();

    Result result;
    result.time = format_iso8601_utc(std::chrono::system_clock::now());

    std::string error;
    if (!m_recorder.start(error)) {
        m_status = std::format("Could not start the recorder: {}", error);
        m_mode = Mode::FromRecorder;
        return false;
    }

    // The system information comes from the recorder.
    const SystemInfo& system = m_recorder.system_info();
    result.info.os_name = system.Common.OsName;
    result.info.os_ver = system.Common.OsVersion;
    result.info.arch = system.Common.Architecture;
    result.info.backend = m_recorder.backend_name();
    if (is_safe_mode(system))
        result.info.safe_mode = true;
    m_result = std::move(result);
    m_has_result = true;
    update_device_ids();
    m_status = "Recording...";
    m_mode = Mode::Recording;
    return true;
}

void App::stop_recording() {
    m_recorder.stop();
    if (m_has_result)
        m_result.info.backend = m_recorder.backend_name();
    m_status = "Recording stopped";
    m_mode = Mode::FromRecorder;
}

void App::set_mode(Mode mode) {
    if (m_mode == mode)
        return;
    if (m_mode == Mode::Recording)
        stop_recording();
    m_mode = mode;
    if (mode == Mode::FromFile)
        m_status = "Open a recording to analyse it";
    else if (mode == Mode::FromRecorder)
        m_status = "Start a recording to analyse it";
}

void App::draw_header() {
    ImGui::BeginTable("##header", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings);
    ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthFixed, 300.0f);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    const auto mode_button = [&](const char* label, Mode mode) {
        const bool active = m_mode == mode || (mode == Mode::FromRecorder && m_mode == Mode::Recording);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(label))
            set_mode(mode);
        if (active)
            ImGui::PopStyleColor();
    };

    mode_button("From file", Mode::FromFile);
    ImGui::SameLine();
    mode_button("From recorder", Mode::FromRecorder);

    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    if (m_has_result && ImGui::Button("Info"))
        m_show_info = true;
    if (m_has_result)
        ImGui::SameLine();
    if (ImGui::Button(ui::theme_name(ui::current_theme())))
        ui::toggle_theme();
    ImGui::SameLine();
    ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);

    ImGui::EndTable();
}

void App::draw_options() {
    ImGui::BeginTable("##options", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings);
    ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthFixed, 260.0f);
    ImGui::TableSetupColumn("##middle", ImGuiTableColumnFlags_WidthFixed, 260.0f);
    ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Binning rate");
    ImGui::SameLine();
    int bin_rate = m_analyzer.bin_rate();
    if (ImGui::BeginCombo("##binrate", std::format("{}", bin_rate).c_str())) {
        for (const int rate : kBinRates) {
            const bool selected = rate == bin_rate;
            if (ImGui::Selectable(std::format("{}Hz", rate).c_str(), selected)) {
                bin_rate = rate;
                m_analyzer.set_bin_rate(rate);
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (m_mode == Mode::Recording && m_analyzer.bin_rate() > 16000) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.1f, 1.0f),
                           "!");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "It is not recommended to go above 16000Hz bin rate during live analysis");
        }
    }

    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Low cut filter");
    ImGui::SameLine();
    ImGui::Checkbox("##lowcut", &m_postprocess.low_cut);

    ImGui::TableNextColumn();
    const ImVec4 color = event_count_color(static_cast<double>(m_analyzer.event_count()));
    ImGui::TextColored(color, "%zu input events", m_analyzer.event_count());
    if (m_analyzer.calculating())
        ImGui::SameLine();
    if (m_analyzer.calculating())
        ImGui::TextColored(ImVec4(0.6f, 0.7f, 1.0f, 1.0f), "calculating...");

    ImGui::EndTable();
}

void App::draw_charts() {
    ImGui::BeginChild("##charts", ImVec2(0.0f, 0.0f),
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImGuiTableFlags table_flags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings;

    ImGui::BeginTable("##diff_charts", 3, table_flags);
    for (int i = 0; i < 3; ++i) {
        ImGui::TableNextColumn();
        switch (i) {
        case 0:
            m_charts.draw_diff_chart("diff_consecutive", "Consecutive timestamp diffs",
                                     m_analyzer.consecutive_diff(), m_analyzer.consecutive_diff_max(),
                                     m_diff_sync);
            break;
        case 1:
            m_charts.draw_diff_chart("diff_all", "All timestamp diffs", m_analyzer.all_diff(),
                                     m_analyzer.all_diff_max(), m_diff_sync);
            break;
        default:
            m_charts.draw_diff_chart("diff_wrapped", "Timestamp fractional parts",
                                     m_analyzer.wrapped_timestamp(),
                                     m_analyzer.wrapped_timestamp_max(), m_diff_sync);
            break;
        }
    }
    ImGui::EndTable();

    ImGui::BeginTable("##freq_charts", 3, table_flags);
    {
        const auto snapshot = m_analyzer.freq_snapshot();
        const PostprocessResult consecutive = postprocess_with_max(
            snapshot ? snapshot->consecutive_diff : std::vector<double>{}, m_postprocess);
        const PostprocessResult all = postprocess_with_max(
            snapshot ? snapshot->all_diff : std::vector<double>{}, m_postprocess);
        const PostprocessResult wrapped = postprocess_with_max(
            snapshot ? snapshot->wrapped_timestamp : std::vector<double>{}, m_postprocess);

        for (int i = 0; i < 3; ++i) {
            ImGui::TableNextColumn();
            switch (i) {
            case 0:
                m_charts.draw_freq_chart("freq_consecutive",
                                         "Consecutive timestamp diffs (frequency domain)",
                                         consecutive.data, consecutive.max, m_analyzer.freq_max_hz(),
                                         m_freq_sync);
                break;
            case 1:
                m_charts.draw_freq_chart("freq_all", "All timestamp diffs (frequency domain)",
                                         all.data, all.max, m_analyzer.freq_max_hz(), m_freq_sync);
                break;
            default:
                m_charts.draw_freq_chart("freq_wrapped",
                                         "Timestamp fractional parts (frequency domain)",
                                         wrapped.data, wrapped.max, m_analyzer.freq_max_hz(),
                                         m_freq_sync);
                break;
            }
        }
    }
    ImGui::EndTable();

    m_charts.draw_input_timeline("##input_timeline", m_timeline, m_device_ids);

    ImGui::EndChild();
}

void App::draw_status() {
    if (m_status.empty())
        return;
    ImGui::TextDisabled("%s", m_status.c_str());
}

void App::draw() {
    if (m_mode == Mode::Recording)
        m_recorder.drain(m_result, m_analyzer, m_timeline);

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Keyboard Inspector", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse |
                     // The main window fills the screen, bringing it to the
                     // front would hide the info window that is drawn after
                     // it.
                     ImGuiWindowFlags_NoBringToFrontOnFocus);

    draw_header();
    ImGui::Separator();
    draw_options();
    ImGui::Separator();

    switch (m_mode) {
    case Mode::FromFile:
        if (ImGui::Button("Open recording...")) {
            if (const auto path = open_recording_dialog(start_directory()))
                open_file(*path);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(.json or legacy .kbi files)");
        break;
    case Mode::FromRecorder:
        if (ImGui::Button("Start recording"))
            start_recording();
        break;
    case Mode::Recording:
        if (ImGui::Button("Stop recording"))
            stop_recording();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "recording...");
        break;
    }

    draw_status();
    ImGui::Separator();

    if (m_has_result)
        draw_charts();

    ImGui::End();

    if (m_show_info)
        ui::draw_info_window(&m_show_info, m_result);
}

} // namespace kbi::app
