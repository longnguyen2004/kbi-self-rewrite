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

namespace kbi::app {

enum class Mode { FromFile, FromRecorder, Recording };

// The application shell: mode selection, the analysis options and the charts.
class App {
public:
    App();

    void draw();

    // Loads a recording from disk (JSON or legacy .kbi).
    bool load(const std::string& path);

    // Starts and stops the embedded recorder.
    bool start_recording();
    void stop_recording();

    // Shows or hides the info window.
    void set_info_visible(bool visible) { m_show_info = visible; }

private:
    void draw_header();
    void draw_options();
    void draw_charts();
    void draw_status();

    void set_mode(Mode mode);
    void open_file(const std::string& path);
    void ingest(const Result& result);
    // The directory the file dialog opens in.
    std::string start_directory() const;
    void reset_analysis();
    void update_device_ids();

    Mode m_mode = Mode::FromFile;

    Analyzer m_analyzer;
    InputTimeline m_timeline;
    Result m_result;
    bool m_has_result = false;

    RecorderSource m_recorder;

    bool m_show_info = false;
    std::string m_status;
    std::string m_last_path;

    PostprocessOptions m_postprocess;
    ui::ChartSync m_diff_sync;
    ui::ChartSync m_freq_sync;
    ui::ChartRenderer m_charts;

    std::vector<std::string> m_device_ids;
};

} // namespace kbi::app
