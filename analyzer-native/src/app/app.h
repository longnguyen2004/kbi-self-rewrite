#pragma once

#include "app/file_dialog.h"
#include "core/analyzer.h"
#include "core/input_timeline.h"
#include "core/postprocess.h"
#include "core/result.h"
#include "recorder/recorder_source.h"
#include "ui/charts.h"
#include "ui/info_window.h"
#include "ui/line_chart.h"
#include "ui/log_store.h"
#include "ui/log_window.h"
#include "ui/timeline_chart.h"

#include <spdlog/fwd.h>

#include <memory>
#include <string>
#include <vector>

namespace Kbi::App {

enum class Mode { FromFile, FromRecorder, Recording };

// The application shell: mode selection, the analysis options and the charts.
class App {
public:
    App(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<Ui::LogStore> logStore);

    void draw();

    // Loads a recording from disk (JSON or legacy .kbi).
    bool load(const std::string& path);

    // Starts and stops the embedded recorder.
    bool startRecording();
    void stopRecording();

    // Shows or hides the info window.
    void setInfoVisible(bool visible) { m_showInfo = visible; }

private:
    void drawHeader();
    void drawOptions();
    void drawCharts();
    void drawStatus();

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
    RecorderSource m_recorder;

    bool m_showInfo = false;
    bool m_showLogs = false;
    std::shared_ptr<Ui::LogStore> m_logStore;
    Ui::LogWindow m_logWindow;
    Ui::InfoWindow m_infoWindow;
    std::string m_status;
    std::string m_lastPath;

    PostprocessOptions m_postprocess;
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
