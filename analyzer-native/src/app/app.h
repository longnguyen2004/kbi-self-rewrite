#pragma once

#include <app/file_dialog.h>
#include <core/analyzer.h>
#include <core/input_timeline.h>
#include <core/postprocess.h>
#include <core/result.h>
#if !defined(__EMSCRIPTEN__)
#include <recorder/recorder_source.h>
#endif
#include <ui/charts.h>
#include <ui/info_window.h>
#include <ui/line_chart.h>
#include <ui/log_store.h>
#include <ui/log_window.h>
#include <ui/timeline_chart.h>

#include <core/rolling_mean.h>

#include <spdlog/fwd.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Kbi::App {

#if defined(__EMSCRIPTEN__)
// Recording is only available in the native build, the web build can just
// analyse files.
enum class Mode { FromFile };
#else
enum class Mode { FromFile, FromRecorder, Recording };
#endif

// The application shell: mode selection, the analysis options and the charts.
class App {
public:
    App(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<Ui::LogStore> logStore);

    void draw();

    // Loads a recording from disk (JSON or legacy .kbi).
    bool load(const std::string& path);

#if !defined(__EMSCRIPTEN__)
    // Starts and stops the embedded recorder.
    bool startRecording();
    void stopRecording();
#endif

    // Shows or hides the info window.
    void setInfoVisible(bool visible) { m_showInfo = visible; }

private:
    void drawHeader();
    void drawOptions();
    void drawCharts();
    void drawStatus();
    void drawStatsWindow();
    void drawPerfsWindow();

    void setMode(Mode mode);
    void openFile(const std::string& path);
    void ingest(const Result& result);
    // The directory the file dialog opens in.
    std::string startDirectory() const;
    void resetAnalysis();
    void updateDeviceIds();

    Mode m_mode = Mode::FromFile;

    Analyzer m_analyzer;
    InputTimeline m_timeline;
    Result m_result;
    bool m_hasResult = false;

    std::shared_ptr<spdlog::logger> m_logger;
#if !defined(__EMSCRIPTEN__)
    RecorderSource m_recorder;
#endif

    bool m_showInfo = false;
    bool m_showLogs = false;
    bool m_showStats = false;
    bool m_showPerfs = false;
    std::shared_ptr<Ui::LogStore> m_logStore;
    Ui::LogWindow m_logWindow;
    Ui::InfoWindow m_infoWindow;
    std::string m_status;
    std::string m_lastPath;

    // Frame durations for the performance window.
    RollingMean m_frameTime;

    PostprocessOptions m_postprocess;
    // The postprocessed spectra of the three frequency charts, their storage
    // is reused across frames (Per.15).
    std::array<PostprocessResult, 3> m_spectra;
    Ui::ChartSync m_diffSync;
    Ui::ChartSync m_freqSync;
    Ui::DiffChart m_diffConsecutive;
    Ui::DiffChart m_diffAll;
    Ui::DiffChart m_diffWrapped;
    Ui::FreqChart m_freqConsecutive;
    Ui::FreqChart m_freqAll;
    Ui::FreqChart m_freqWrapped;

    std::vector<std::string> m_deviceIds;

    Ui::TimelineChart m_timelineChart;
};

} // namespace Kbi::App
