#include "app/app.h"

#include "app/theme.h"
#include "parser/parser.h"

#include "ui/info_window.h"
#include <datetime.h>
#include <chrono>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <format>

namespace Kbi::App {
namespace {

constexpr std::array<int, 8> kBinRates = { 1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000 };

// Piecewise linear interpolation between the event count colors.
ImVec4 eventCountColor(double value)
{
    static constexpr std::array<double, 4> stops = { 50.0, 200.0, 600.0, 1500.0 };
    static constexpr std::array<ImVec4, 4> colors = {
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
            return {
                colors[i].x + static_cast<float>(t) * (colors[i + 1].x - colors[i].x),
                colors[i].y + static_cast<float>(t) * (colors[i + 1].y - colors[i].y),
                colors[i].z + static_cast<float>(t) * (colors[i + 1].z - colors[i].z), 1.0f
            };
        }
    }
    return colors[0];
}

} // namespace

App::App(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<Ui::LogStore> logStore)
    : m_logger(std::move(logger))
    , m_recorder(m_logger)
    , m_logStore(std::move(logStore))
{
    resetAnalysis();
}

void App::resetAnalysis()
{
    m_analyzer.reset();
    m_timeline.reset();
    m_result = Result {};
    m_hasResult = false;
    m_deviceIds.clear();
    m_infoWindow.reset();
    m_diffSync = Ui::ChartSync {};
    m_freqSync = Ui::ChartSync {};
    m_charts.resetZoom();
}

// The directory the file dialog opens in.
std::string App::startDirectory() const
{
    if (m_lastPath.empty())
        return ".";
    const std::filesystem::path parent = std::filesystem::path(m_lastPath).parent_path();
    if (parent.empty())
        return ".";
    return parent.string();
}

void App::updateDeviceIds()
{
    m_deviceIds.clear();
    m_deviceIds.reserve(m_result.devices.size());
    for (const auto& [id, device] : m_result.devices)
        m_deviceIds.push_back(id);
}

void App::ingest(const Result& result)
{
    m_result = result;
    m_hasResult = true;

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

    updateDeviceIds();
}

void App::openFile(const std::string& path)
{
    const auto parsed = parseResultFile(path);
    if (!parsed) {
        m_status = std::format("Could not read {}", path);
        m_logger->error("Could not read {}", path);
        return;
    }
    resetAnalysis();
    ingest(*parsed);
    // The timeline starts at the beginning of the recording.
    m_charts.setTimelineRange(0.0, 10.0);
    m_lastPath = path;
    m_status = std::format("Loaded {}", path);
    m_logger->info("Loaded {}", path);
}

bool App::load(const std::string& path)
{
    openFile(path);
    return m_hasResult;
}

bool App::startRecording()
{
    resetAnalysis();

    Result result;
    result.time = formatIso8601Utc(std::chrono::system_clock::now());

    std::string error;
    if (!m_recorder.start(error)) {
        m_logger->error("Could not start the recorder: {}", error);
        m_status = std::format("Could not start the recorder: {}", error);
        m_mode = Mode::FromRecorder;
        return false;
    }

    // The system information comes from the recorder.
    const SystemInfo& system = m_recorder.systemInfo();
    result.info.osName = system.common.osName;
    result.info.osVer = system.common.osVersion;
    result.info.arch = system.common.architecture;
    result.info.backend = m_recorder.backendName();
    if (isSafeMode(system))
        result.info.safeMode = true;
    m_result = std::move(result);
    m_hasResult = true;
    updateDeviceIds();
    m_status = "Recording...";
    m_mode = Mode::Recording;
    return true;
}

void App::stopRecording()
{
    m_recorder.stop();
    if (m_hasResult)
        m_result.info.backend = m_recorder.backendName();
    m_status = "Recording stopped";
    m_logger->info("Recording stopped, {} events captured", m_result.totalInputCount());
    m_mode = Mode::FromRecorder;
}

void App::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    const bool wasRecording = m_mode == Mode::Recording;
    if (wasRecording)
        stopRecording();
    m_mode = mode;
    // Switching between the sources starts from scratch, the result of the
    // previous one does not belong to them. A recording that is stopped keeps
    // its data around, so that it can still be looked at.
    if (!wasRecording || mode != Mode::FromRecorder)
        resetAnalysis();
    if (mode == Mode::FromFile)
        m_status = "Open a recording to analyse it";
    else if (mode == Mode::FromRecorder)
        m_status = "Start a recording to analyse it";
}

void App::drawHeader()
{
    ImGui::BeginTable("##header", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings);
    ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthFixed, 340.0f);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    const auto modeButton = [&](const char* label, Mode mode) {
        const bool active = m_mode == mode || (mode == Mode::FromRecorder && m_mode == Mode::Recording);
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(label))
            setMode(mode);
        if (active)
            ImGui::PopStyleColor();
    };

    modeButton("From file", Mode::FromFile);
    ImGui::SameLine();
    modeButton("From recorder", Mode::FromRecorder);

    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    if (m_hasResult && ImGui::Button("Info"))
        m_showInfo = true;
    if (m_hasResult)
        ImGui::SameLine();
    if (ImGui::Button("Logs"))
        m_showLogs = !m_showLogs;
    ImGui::SameLine();
    if (ImGui::Button(Ui::themeName(Ui::currentTheme())))
        Ui::toggleTheme();
    ImGui::SameLine();
    ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);

    ImGui::EndTable();
}

void App::drawOptions()
{
    ImGui::BeginTable("##options", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings);
    ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthFixed, 260.0f);
    ImGui::TableSetupColumn("##middle", ImGuiTableColumnFlags_WidthFixed, 260.0f);
    ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Binning rate");
    ImGui::SameLine();
    int binRate = m_analyzer.binRate();
    if (ImGui::BeginCombo("##binrate", std::format("{}", binRate).c_str())) {
        for (const int rate : kBinRates) {
            const bool selected = rate == binRate;
            if (ImGui::Selectable(std::format("{}Hz", rate).c_str(), selected)) {
                binRate = rate;
                m_analyzer.setBinRate(rate);
            }
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    if (m_mode == Mode::Recording && m_analyzer.binRate() > 16000) {
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
    ImGui::Checkbox("##lowcut", &m_postprocess.lowCut);

    ImGui::TableNextColumn();
    const ImVec4 color = eventCountColor(static_cast<double>(m_analyzer.eventCount()));
    ImGui::TextColored(color, "%zu input events", m_analyzer.eventCount());
    if (m_analyzer.calculating())
        ImGui::SameLine();
    if (m_analyzer.calculating())
        ImGui::TextColored(ImVec4(0.6f, 0.7f, 1.0f, 1.0f), "calculating...");

    ImGui::EndTable();
}

void App::drawCharts()
{
    ImGui::BeginChild("##charts", ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings;

    ImGui::BeginTable("##diff_charts", 3, tableFlags);
    for (int i = 0; i < 3; ++i) {
        ImGui::TableNextColumn();
        switch (i) {
        case 0:
            m_charts.drawDiffChart("diff_consecutive", "Consecutive timestamp diffs",
                m_analyzer.consecutiveDiff(), m_analyzer.consecutiveDiffMax(),
                m_diffSync);
            break;
        case 1:
            m_charts.drawDiffChart("diff_all", "All timestamp diffs", m_analyzer.allDiff(),
                m_analyzer.allDiffMax(), m_diffSync);
            break;
        default:
            m_charts.drawDiffChart("diff_wrapped", "Timestamp fractional parts",
                m_analyzer.wrappedTimestamp(),
                m_analyzer.wrappedTimestampMax(), m_diffSync);
            break;
        }
    }
    ImGui::EndTable();

    ImGui::BeginTable("##freq_charts", 3, tableFlags);
    {
        const auto snapshot = m_analyzer.freqSnapshot();
        const PostprocessResult consecutive = postprocessWithMax(
            snapshot ? snapshot->consecutiveDiff : std::vector<double> {}, m_postprocess);
        const PostprocessResult all = postprocessWithMax(
            snapshot ? snapshot->allDiff : std::vector<double> {}, m_postprocess);
        const PostprocessResult wrapped = postprocessWithMax(
            snapshot ? snapshot->wrappedTimestamp : std::vector<double> {}, m_postprocess);

        for (int i = 0; i < 3; ++i) {
            ImGui::TableNextColumn();
            switch (i) {
            case 0:
                m_charts.drawFreqChart("freq_consecutive",
                    "Consecutive timestamp diffs (frequency domain)",
                    consecutive.data, consecutive.max, m_analyzer.freqMaxHz(),
                    m_freqSync);
                break;
            case 1:
                m_charts.drawFreqChart("freq_all", "All timestamp diffs (frequency domain)",
                    all.data, all.max, m_analyzer.freqMaxHz(), m_freqSync);
                break;
            default:
                m_charts.drawFreqChart("freq_wrapped",
                    "Timestamp fractional parts (frequency domain)",
                    wrapped.data, wrapped.max, m_analyzer.freqMaxHz(),
                    m_freqSync);
                break;
            }
        }
    }
    ImGui::EndTable();

    m_charts.drawInputTimeline("##input_timeline", m_timeline, m_deviceIds);

    ImGui::EndChild();
}

void App::drawStatus()
{
    if (m_status.empty())
        return;
    ImGui::TextDisabled("%s", m_status.c_str());
}

void App::draw()
{
    if (m_mode == Mode::Recording) {
        m_recorder.drain(m_result, m_analyzer, m_timeline);
        // The recorder discovers the devices while recording, their colors
        // are assigned in the order they appear.
        if (m_result.devices.size() != m_deviceIds.size())
            updateDeviceIds();
    }

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Keyboard Inspector", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            // The main window fills the screen, bringing it to the
            // front would hide the info window that is drawn after
            // it.
            ImGuiWindowFlags_NoBringToFrontOnFocus);

    drawHeader();
    ImGui::Separator();
    drawOptions();
    ImGui::Separator();

    switch (m_mode) {
    case Mode::FromFile:
        if (ImGui::Button("Open recording...")) {
            if (const auto path = openRecordingDialog(startDirectory()))
                openFile(*path);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(.json or legacy .kbi files)");
        break;
    case Mode::FromRecorder:
        if (ImGui::Button("Start recording"))
            startRecording();
        break;
    case Mode::Recording:
        if (ImGui::Button("Stop recording"))
            stopRecording();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "recording...");
        break;
    }

    drawStatus();
    ImGui::Separator();

    if (m_hasResult)
        drawCharts();

    ImGui::End();

    if (m_showInfo)
        m_infoWindow.draw(&m_showInfo, m_result);
    if (m_showLogs && m_logStore)
        m_logWindow.draw(&m_showLogs, *m_logStore);
}

} // namespace Kbi::app
