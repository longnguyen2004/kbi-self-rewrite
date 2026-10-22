#pragma once

#include "app/file_dialog.h"
#include "core/analyzer.h"
#include "core/input_timeline.h"
#include "core/postprocess.h"
#include "core/result.h"
#include "recorder/recorder_source.h"
#include "ui/charts.h"

#include <string>
#include <vector>

namespace Kbi::App {

enum class Mode { FromFile,
    FromRecorder,
    Recording };

// The application shell: mode selection, the analysis options and the charts.
class App {
public:
    App();

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

    RecorderSource m_recorder;

    bool m_showInfo = false;
    std::string m_status;
    std::string m_lastPath;

    PostprocessOptions m_postprocess;
    Ui::ChartSync m_diffSync;
    Ui::ChartSync m_freqSync;
    Ui::ChartRenderer m_charts;

    std::vector<std::string> m_deviceIds;
};

} // namespace Kbi::app
