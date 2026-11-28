#include <app/app.h>

#include <app/save_recording.h>
#include <app/theme.h>
#include <core/format.h>
#include <parser/parser.h>

#include <datetime.h>
#include <chrono>
#include <ui/info_window.h>

#include <imgui.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <format>
#include <string_view>

namespace Kbi::App {
namespace {

constexpr std::array<int, 8> kBinRates = { 1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000 };

#if !defined(__EMSCRIPTEN__)
// The bin rate the analyzer defaults to; live analysis above it is not
// recommended (see the tooltip in drawControls).
constexpr int kDefaultBinRate = 16000;
#endif

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
            return { colors[i].x + static_cast<float>(t) * (colors[i + 1].x - colors[i].x),
                colors[i].y + static_cast<float>(t) * (colors[i + 1].y - colors[i].y),
                colors[i].z + static_cast<float>(t) * (colors[i + 1].z - colors[i].z), 1.0f };
        }
    }
    return colors[0];
}

// Draws the two shortest durations of the recording: the shortest keypress
// and the shortest gap between two events.
void drawShortestDurations(const InputTimeline& timeline, const Analyzer& analyzer)
{
    if (const double shortestKeypress = timeline.shortestKeypressDuration();
        std::isfinite(shortestKeypress))
        ImGui::Text("Shortest keypress duration: %.3fs", shortestKeypress);
    if (const double shortestGap = analyzer.shortestConsecutiveGap(); std::isfinite(shortestGap))
        ImGui::Text("Shortest time between events: %s", formatDuration(shortestGap).c_str());
}

} // namespace

App::App(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<Ui::LogStore> logStore)
    : m_logger(std::move(logger))
#if !defined(__EMSCRIPTEN__)
    , m_recorder(m_logger)
#endif
    , m_logStore(std::move(logStore))
    , m_diffConsecutive("diff_consecutive", "Consecutive timestamp diffs", &m_diffSync)
    , m_diffAll("diff_all", "All timestamp diffs", &m_diffSync)
    , m_diffWrapped("diff_wrapped", "Timestamp fractional parts", &m_diffSync)
    , m_freqConsecutive(
          "freq_consecutive", "Consecutive timestamp diffs (frequency domain)", &m_freqSync)
    , m_freqAll("freq_all", "All timestamp diffs (frequency domain)", &m_freqSync)
    , m_freqWrapped("freq_wrapped", "Timestamp fractional parts (frequency domain)", &m_freqSync)
    , m_timelineChart(m_timeline, m_deviceIds)
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
    m_timelineChart.reset();
    m_timelineChart.resetZoom();
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
    // Like the web app, the timeline opens showing the whole recording: the
    // reset in resetAnalysis() makes the next draw initialize the view to the
    // full extent.
    m_lastPath = path;
    m_status = std::format("Loaded {}", path);
    m_logger->info("Loaded {}", path);
}

bool App::load(const std::string& path)
{
    openFile(path);
    return m_hasResult;
}

#if !defined(__EMSCRIPTEN__)
bool App::startRecording()
{
    resetAnalysis();

    Result result;
    result.time = formatIso8601Utc(std::chrono::system_clock::now());

    if (const auto error = m_recorder.start()) {
        m_logger->error("Could not start the recorder: {}", *error);
        m_status = std::format("Could not start the recorder: {}", *error);
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
    m_status = "Recording stopped";
    m_logger->info("Recording stopped, {} events captured", m_result.totalInputCount());
    m_mode = Mode::FromRecorder;
}

void App::saveRecording()
{
    const auto path = saveRecordingDialog(startDirectory());
    if (!path)
        return;

    const Recorder* recorder = m_recorder.recorder();
    if (recorder == nullptr) {
        m_status = "No recording to save";
        m_logger->error("No recording to save");
        return;
    }

    if (const auto error = saveRecordingToFile(*recorder, *path)) {
        m_status = *error;
        m_logger->error("{}", *error);
        return;
    }

    m_lastPath = *path;
    m_status = std::format("Saved {} (JSON and .kbi)", *path);
    m_logger->info("{}", m_status);
}
#endif

void App::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
#if !defined(__EMSCRIPTEN__)
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
#else
    m_mode = mode;
    resetAnalysis();
    m_status = "Open a recording to analyse it";
#endif
}

void App::drawHeader()
{
    ImGui::BeginTable(
        "##header", 2, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings);
    ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("##right");
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    const auto modeButton = [&](std::string_view label, Mode mode) {
#if defined(__EMSCRIPTEN__)
        const bool active = m_mode == mode;
#else
        const bool active
            = m_mode == mode || (mode == Mode::FromRecorder && m_mode == Mode::Recording);
#endif
        if (active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        // The button labels are literals, they are NUL-terminated.
        // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
        if (ImGui::Button(label.data()))
            setMode(mode);
        if (active)
            ImGui::PopStyleColor();
    };

    modeButton("From file", Mode::FromFile);
#if !defined(__EMSCRIPTEN__)
    ImGui::SameLine();
    modeButton("From recorder", Mode::FromRecorder);
#endif

    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    if (m_hasResult && ImGui::Button("Info"))
        m_showInfo = true;
    if (m_hasResult)
        ImGui::SameLine();
    if (ImGui::Button("Logs"))
        m_showLogs = !m_showLogs;
    ImGui::SameLine();
    // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage)
    if (ImGui::Button(Ui::themeName(Ui::currentTheme()).data()))
        Ui::toggleTheme();
    ImGui::SameLine();
    if (ImGui::Button("Perfs"))
        m_showPerfs = true;

    ImGui::EndTable();
}

void App::drawOptions()
{
    ImGui::BeginTable(
        "##options", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings);
    ImGui::TableSetupColumn("##left", ImGuiTableColumnFlags_WidthFixed, 260.0f);
    ImGui::TableSetupColumn("##middle", ImGuiTableColumnFlags_WidthFixed, 260.0f);
    ImGui::TableSetupColumn("##right", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Binning rate");
    ImGui::SameLine();
    int binRate = m_analyzer.binRate();
    if (ImGui::BeginCombo("##binrate", std::format("{}Hz", binRate).c_str())) {
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
#if !defined(__EMSCRIPTEN__)
    if (m_mode == Mode::Recording && m_analyzer.binRate() > kDefaultBinRate) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.1f, 1.0f), "!");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "It is not recommended to go above 16000Hz bin rate during live analysis");
        }
    }
#endif

    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Low cut filter");
    ImGui::SameLine();
    ImGui::Checkbox("##lowcut", &m_postprocess.lowCut);

    ImGui::TableNextColumn();
    const ImVec4 color = eventCountColor(static_cast<double>(m_analyzer.eventCount()));
    ImGui::TextColored(color, "%zu input events", m_analyzer.eventCount());
    ImGui::SameLine();
    if (ImGui::Button("More"))
        m_showStats = true;
    if (m_analyzer.calculating())
        ImGui::SameLine();
    if (m_analyzer.calculating())
        ImGui::TextColored(ImVec4(0.6f, 0.7f, 1.0f, 1.0f), "calculating...");

    ImGui::EndTable();
}

void App::drawCharts()
{
    ImGui::BeginChild("##charts", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Split the remaining height into three equal rows: two tables of titled
    // charts and the timeline. Leave one text line at the bottom for ImPlot's
    // x-axis label and for the gaps between the rows, otherwise the last plot
    // would overflow the child and clip its "s" label. The plot sizes are
    // derived from the available space and ImGui's own metrics, so they adapt
    // to window size and display scale instead of being fixed pixel values.
    const float textHeight = ImGui::GetTextLineHeightWithSpacing();
    const float rowHeight = (ImGui::GetContentRegionAvail().y - textHeight) / 3.0f;
    const float cellPadding = 2.0f * ImGui::GetStyle().CellPadding.y;
    const float titledPlotHeight = std::max(rowHeight - textHeight - cellPadding, textHeight);

    constexpr ImGuiTableFlags tableFlags
        = ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings;

    // The three diff charts iterate over the three histograms of the
    // analyzer (F.1 — one loop instead of a copied switch per chart kind).
    const std::array<Ui::DiffChart*, 3> diffCharts
        = { &m_diffConsecutive, &m_diffAll, &m_diffWrapped };
    for (Ui::DiffChart* chart : diffCharts)
        chart->setChartHeight(titledPlotHeight);
    const std::array<std::pair<const std::vector<double>*, double>, 3> diffInputs
        = { { { &m_analyzer.consecutiveDiffCount(), m_analyzer.consecutiveDiffCountMax() },
            { &m_analyzer.allDiffCount(), m_analyzer.allDiffCountMax() },
            { &m_analyzer.wrappedTimestampCount(), m_analyzer.wrappedTimestampCountMax() } } };

    ImGui::BeginTable("##diff_charts", 3, tableFlags);
    for (int i = 0; i < 3; ++i) {
        const auto column = static_cast<std::size_t>(i);
        ImGui::TableNextColumn();
        diffCharts[column]->draw(*diffInputs[column].first, diffInputs[column].second);
    }
    ImGui::EndTable();

    const std::array<Ui::FreqChart*, 3> freqCharts
        = { &m_freqConsecutive, &m_freqAll, &m_freqWrapped };
    for (Ui::FreqChart* chart : freqCharts)
        chart->setChartHeight(titledPlotHeight);
    const auto snapshot = m_analyzer.freqSnapshot();
    const std::vector<double> emptySpectrum;
    const std::array<const std::vector<double>*, 3> spectraInput = snapshot
        ? std::array<const std::vector<double>*, 3> { &snapshot->consecutiveDiff,
              &snapshot->allDiff, &snapshot->wrappedTimestamp }
        : std::array<const std::vector<double>*, 3> { &emptySpectrum, &emptySpectrum,
              &emptySpectrum };
    for (int i = 0; i < 3; ++i)
        postprocessWithMax(*spectraInput[static_cast<std::size_t>(i)], m_postprocess,
            m_spectra[static_cast<std::size_t>(i)]);

    ImGui::BeginTable("##freq_charts", 3, tableFlags);
    for (int i = 0; i < 3; ++i) {
        const auto column = static_cast<std::size_t>(i);
        ImGui::TableNextColumn();
        freqCharts[column]->draw(
            m_spectra[column].data, m_spectra[column].max, m_analyzer.freqMaxHz());
    }
    ImGui::EndTable();

    m_timelineChart.setChartHeight(rowHeight);
    m_timelineChart.draw();

    ImGui::EndChild();
}

void App::drawStatus()
{
    if (m_status.empty())
        return;
    ImGui::TextDisabled("%s", m_status.c_str());
}

void App::drawStatsWindow()
{
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Statistics", &m_showStats, ImGuiWindowFlags_NoCollapse))
        return;

    drawShortestDurations(m_timeline, m_analyzer);

    ImGui::End();
}

void App::drawPerfsWindow()
{
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Perfs", &m_showPerfs, ImGuiWindowFlags_NoCollapse))
        return;

    if (!m_frameTime.empty()) {
        const double averageFrameTime = m_frameTime.mean();
        ImGui::Text("Average frametime: %.3f ms", averageFrameTime * 1000.0);
        ImGui::Text("Framerate: %.1f FPS", 1.0 / averageFrameTime);
    } else {
        ImGui::TextDisabled("No frametime samples yet");
    }
    ImGui::Text("Average FFT time: %.3f ms", m_analyzer.averageFftTimeMs());

    ImGui::End();
}

void App::draw()
{
    // ImGui reports the time since the previous frame in seconds. Ignore a
    // zero delta (the first frame) so it cannot skew the framerate.
    if (const double delta = static_cast<double>(ImGui::GetIO().DeltaTime); delta > 0.0)
        m_frameTime.add(delta);

#if !defined(__EMSCRIPTEN__)
    if (m_mode == Mode::Recording) {
        m_recorder.drain(m_result, m_analyzer, m_timeline);
        // The recorder discovers the devices while recording, their colors
        // are assigned in the order they appear.
        if (m_result.devices.size() != m_deviceIds.size())
            updateDeviceIds();
    }
#endif

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    // The main window is the backdrop: keep it opaque, only the dialogs are
    // see-through (the style sets a transparent window background).
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::Begin("Keyboard Inspector", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoScrollWithMouse |
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
#if !defined(__EMSCRIPTEN__)
    case Mode::FromRecorder:
        if (ImGui::Button("Start recording"))
            startRecording();
        // Only a stopped recording has stable data to serialize; the
        // recorder's maps are still being written to while it runs.
        if (m_hasResult) {
            ImGui::SameLine();
            if (ImGui::Button("Save recording..."))
                saveRecording();
        }
        break;
    case Mode::Recording:
        if (ImGui::Button("Stop recording"))
            stopRecording();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "recording...");
        break;
#endif
    }

    drawStatus();
    ImGui::Separator();

    if (m_hasResult)
        drawCharts();

    ImGui::End();

    if (m_showInfo)
        m_infoWindow.draw(m_showInfo, m_result);
    if (m_showLogs && m_logStore)
        m_logWindow.draw(m_showLogs, *m_logStore);
    if (m_showStats)
        drawStatsWindow();
    if (m_showPerfs)
        drawPerfsWindow();
}

} // namespace Kbi::App
