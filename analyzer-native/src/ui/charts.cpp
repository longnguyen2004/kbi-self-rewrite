#include "ui/charts.h"

#include "core/decimate.h"
#include "ui/point_snapper.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <format>
#include <map>
#include <string_view>
#include <set>

namespace kbi::ui {
namespace {

constexpr ImVec4 kDiffColor(65.0f / 255.0f, 140.0f / 255.0f, 240.0f / 255.0f, 1.0f);
constexpr ImVec4 kFreqColor(240.0f / 255.0f, 120.0f / 255.0f, 0.0f, 1.0f);

// Extra room around the plot area that still counts as "over the chart".
constexpr float kHoverMargin = 16.0f;

const ImVec4* device_palette() {
    static const ImVec4 palette[] = {
        ImVec4(65.0f / 255.0f, 140.0f / 255.0f, 240.0f / 255.0f, 1.0f),
        ImVec4(240.0f / 255.0f, 120.0f / 255.0f, 0.0f, 1.0f),
        ImVec4(80.0f / 255.0f, 200.0f / 255.0f, 120.0f / 255.0f, 1.0f),
        ImVec4(220.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f, 1.0f),
        ImVec4(180.0f / 255.0f, 100.0f / 255.0f, 220.0f / 255.0f, 1.0f),
        ImVec4(240.0f / 255.0f, 200.0f / 255.0f, 60.0f / 255.0f, 1.0f),
    };
    return palette;
}

// The tick positions of the difference charts: the range is covered with
// steps of 0.125, 0.25, 0.5, 1, 2, 5, ... units.
std::vector<double> millisecond_ticks(double min, double max) {
    const double range = max - min;
    double step = 200.0;
    if (range <= 1.5)
        step = 0.125;
    else if (range <= 3.0)
        step = 0.25;
    else if (range <= 5.0)
        step = 0.5;
    else if (range <= 10.0)
        step = 1.0;
    else if (range <= 20.0)
        step = 2.0;
    else if (range <= 50.0)
        step = 5.0;
    else if (range <= 100.0)
        step = 10.0;
    else if (range <= 200.0)
        step = 20.0;
    else if (range <= 500.0)
        step = 50.0;
    else if (range <= 1000.0)
        step = 100.0;

    const double quantized_min = std::floor(min / step + 1.0) * step;
    const double quantized_max = std::ceil(max / step - 1.0) * step;

    std::vector<double> ticks;
    ticks.push_back(min);
    for (double value = quantized_min; value <= quantized_max; value += step)
        ticks.push_back(value);
    ticks.push_back(max);
    return ticks;
}

// The tick positions of the frequency charts: 125Hz, 250Hz, 500Hz, ...
std::vector<double> hertz_ticks(double min, double max) {
    const auto steps = static_cast<std::uint64_t>(std::floor((max - min) / 1000.0));
    const double step = std::clamp(125.0 * static_cast<double>(std::bit_ceil(steps)), 125.0, 8000.0);
    const double quantized_min = std::floor(min / step + 1.0) * step;
    const double quantized_max = std::ceil(max / step - 1.0) * step;

    std::vector<double> ticks;
    ticks.push_back(min);
    for (double value = quantized_min; value <= quantized_max; value += step)
        ticks.push_back(value);
    ticks.push_back(max);
    return ticks;
}

// The radius of the hit test around the cursor, in pixels.
constexpr double kTooltipRadius = 40.0;
// The margin between the bars of the input timeline and the edges of their
// row, in pixels.
constexpr double kBarMargin = 3.0;
// The distance of the tooltip above the point that it describes.
constexpr float kTooltipOffset = 12.0f;

// Shows the tooltip of a data point, anchored above it.
void show_tooltip(const ImVec2& anchor, const std::vector<std::string>& lines) {
    // The size of the tooltip is estimated, so that it can be kept inside the
    // viewport. The web analyzer does the same with its tooltip placement.
    ImVec2 size(0.0f, 0.0f);
    for (const std::string& line : lines) {
        const ImVec2 line_size = ImGui::CalcTextSize(line.c_str());
        size.x = std::max(size.x, line_size.x);
        size.y += line_size.y;
    }
    const ImGuiStyle& style = ImGui::GetStyle();
    size.x += style.WindowPadding.x * 2.0f;
    size.y += style.WindowPadding.y * 2.0f;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float left = std::clamp(anchor.x - size.x * 0.5f, viewport->WorkPos.x,
                                  viewport->WorkPos.x + viewport->WorkSize.x - size.x);
    const float top = std::clamp(anchor.y - kTooltipOffset - size.y, viewport->WorkPos.y,
                                 viewport->WorkPos.y + viewport->WorkSize.y - size.y);

    ImGui::SetNextWindowPos(ImVec2(left + size.x * 0.5f, top + size.y), ImGuiCond_Always,
                            ImVec2(0.5f, 1.0f));
    if (ImGui::BeginTooltip()) {
        for (const std::string& line : lines)
            ImGui::TextUnformatted(line.c_str());
        ImGui::EndTooltip();
    }
}

// The area of the plot, and whether the mouse is over it.
struct PlotArea {
    ImVec2 position{0.0f, 0.0f};
    ImVec2 size{0.0f, 0.0f};
    bool hovered = false;
    double mouse_x = 0.0; // mouse position in data units
};

PlotArea current_plot_area(double x_min, double x_max) {
    PlotArea area;
    area.position = ImPlot::GetPlotPos();
    area.size = ImPlot::GetPlotSize();

    const ImVec2 mouse = ImGui::GetMousePos();
    const ImVec2 min = ImVec2(area.position.x - kHoverMargin, area.position.y - kHoverMargin);
    const ImVec2 max = ImVec2(area.position.x + area.size.x + kHoverMargin,
                              area.position.y + area.size.y + kHoverMargin);
    // A window that is on top of the chart (the info popup for example) takes
    // the hover away from it, the chart must not react to the mouse then.
    area.hovered = ImGui::IsMouseHoveringRect(min, max) &&
                   ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

    if (area.size.x > 1.0f) {
        const double t = (mouse.x - area.position.x) / area.size.x;
        area.mouse_x = x_min + t * (x_max - x_min);
    } else {
        area.mouse_x = x_min;
    }
    return area;
}

// Converts a data position into a pixel position inside the plot.
double to_pixel_x(const PlotArea& area, double x_min, double x_max, double x) {
    const double span = x_max - x_min;
    if (span <= 0.0)
        return area.position.x;
    return area.position.x + (x - x_min) / span * area.size.x;
}

double to_pixel_y(const PlotArea& area, double y_min, double y_max, double y) {
    const double span = y_max - y_min;
    if (span <= 0.0)
        return area.position.y + area.size.y;
    return area.position.y + area.size.y * (1.0 - (y - y_min) / span);
}

// Handles panning, zooming and resetting. The range is stored in the sync
// object and applied on the next frame.
void handle_interaction(ChartSync& sync, const PlotArea& area) {
    if (!area.hovered)
        return;

    ChartInteraction interaction;
    interaction.dragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left);
    interaction.drag_delta_x = ImGui::GetIO().MouseDelta.x;
    interaction.wheel = ImGui::GetIO().MouseWheel;
    interaction.mouse_x = area.mouse_x;
    interaction.double_clicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    interaction.plot_width = area.size.x;

    update_range(sync, interaction);
}

} // namespace

void update_range(ChartSync& sync, const ChartInteraction& interaction) {
    const double span = sync.x_max - sync.x_min;
    const double extent_span = std::max(sync.extent_max - sync.extent_min, 1e-12);
    const double min_span = std::max(extent_span * 1e-9, 1e-12);

    if (interaction.dragging && interaction.plot_width > 1.0f) {
        const double delta =
            -static_cast<double>(interaction.drag_delta_x) * span / interaction.plot_width;
        double new_min = sync.x_min + delta;
        double new_max = sync.x_max + delta;
        // Keep the view inside the data extent.
        if (new_min < sync.extent_min) {
            new_min = sync.extent_min;
            new_max = new_min + span;
        }
        if (new_max > sync.extent_max) {
            new_max = sync.extent_max;
            new_min = new_max - span;
        }
        sync.x_min = new_min;
        sync.x_max = new_max;
    }

    if (interaction.wheel != 0.0f) {
        const double factor = std::pow(0.9, static_cast<double>(interaction.wheel));
        // The cursor is the anchor of the zoom. When it is outside of the
        // visible range it is clamped to the closest edge, otherwise the range
        // would collapse.
        const double anchor = std::clamp(interaction.mouse_x, sync.x_min, sync.x_max);
        double new_min = anchor - (anchor - sync.x_min) * factor;
        double new_max = anchor + (sync.x_max - anchor) * factor;

        const double new_span = new_max - new_min;
        if (new_span < min_span) {
            const double center = (new_min + new_max) * 0.5;
            new_min = center - min_span * 0.5;
            new_max = center + min_span * 0.5;
        }
        // The view stays inside the extent of the data.
        if (new_span > extent_span) {
            new_min = sync.extent_min;
            new_max = sync.extent_max;
        } else {
            if (new_min < sync.extent_min) {
                new_min = sync.extent_min;
                new_max = new_min + new_span;
            }
            if (new_max > sync.extent_max) {
                new_max = sync.extent_max;
                new_min = new_max - new_span;
            }
        }
        sync.x_min = new_min;
        sync.x_max = new_max;
    }

    if (interaction.double_clicked) {
        sync.x_min = sync.extent_min;
        sync.x_max = sync.extent_max;
    }
}

void ChartRenderer::draw_line_chart(const char* id,
                                    const char* title,
                                    const std::vector<double>& data,
                                    double y_max,
                                    ChartSync& sync,
                                    const ImVec4& color,
                                    const char* x_label,
                                    const char* y_label,
                                    double initial_span,
                                    TickStyle tick_style,
                                    TooltipStyle tooltip_style) {
    const double extent_min = sync.extent_min;
    const double extent_max = sync.extent_max;
    const double extent_span = std::max(extent_max - extent_min, 1e-12);

    if (!sync.initialized || sync.data_length != data.size()) {
        sync.initialized = true;
        sync.data_length = data.size();
        sync.x_min = extent_min;
        sync.x_max = extent_min + std::min(initial_span, extent_span);
    }
    sync.x_min = std::clamp(sync.x_min, extent_min, extent_max);
    sync.x_max = std::clamp(sync.x_max, extent_min, extent_max);
    if (sync.x_max <= sync.x_min)
        sync.x_max = std::min(extent_max, sync.x_min + std::max(extent_span * 1e-9, 1e-12));

    ImGui::PushID(id);
    if (title)
        ImGui::TextUnformatted(title);

    // ImPlot's own input handling is disabled, the interactions below are
    // implemented explicitly so that they behave the same in every chart.
    if (!ImPlot::BeginPlot("##plot", ImVec2(-1.0f, m_chart_height),
                            ImPlotFlags_NoTitle | ImPlotFlags_NoInputs)) {
        ImPlot::EndPlot();
        ImGui::PopID();
        return;
    }

    // The y axis goes from zero to the maximum of the data, like in the web
    // analyzer. Only an empty chart falls back to a range of one.
    const double y_range = y_max > 0.0 ? y_max : 1.0;

    ImPlot::SetupAxis(ImAxis_X1, x_label);
    ImPlot::SetupAxis(ImAxis_Y1, y_label);
    ImPlot::SetupAxisLimits(ImAxis_X1, sync.x_min, sync.x_max, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, y_range, ImPlotCond_Always);

    // Custom ticks, generated the same way as in the web analyzer.
    if (tick_style != TickStyle::Automatic) {
        const std::vector<double> generated = tick_style == TickStyle::Milliseconds
                                                  ? millisecond_ticks(sync.x_min, sync.x_max)
                                                  : hertz_ticks(sync.x_min, sync.x_max);
        std::vector<double> values;
        std::vector<std::string> label_storage;
        for (const double value : generated) {
            std::string label;
            if (tick_style == TickStyle::Milliseconds) {
                const double fractional = value - std::floor(value);
                // Only multiples of an eighth are labelled.
                if (std::floor(fractional * 8.0) - fractional * 8.0 != 0.0)
                    continue;
                if (fractional == 0.0)
                    label = std::format("{}ms", value);
                else
                    label = std::format("{:.3f}", fractional + 1.0);
            } else {
                if (value != std::floor(value))
                    continue;
                label = std::format("{}Hz", value);
            }
            values.push_back(value);
            label_storage.push_back(std::move(label));
        }
        std::vector<const char*> labels;
        labels.reserve(label_storage.size());
        for (const std::string& label : label_storage)
            labels.push_back(label.c_str());
        ImPlot::SetupAxisTicks(ImAxis_X1, values.data(), static_cast<int>(values.size()),
                               labels.data());
    }

    const PlotArea area = current_plot_area(sync.x_min, sync.x_max);
    const int width = static_cast<int>(area.size.x);

    std::vector<ChartPoint> points;
    if (!data.empty() && width > 0)
        points = decimate(data, extent_min, extent_max, sync.x_min, sync.x_max, width);

    if (!points.empty()) {
        std::vector<double> xs;
        std::vector<double> ys;
        xs.reserve(points.size());
        ys.reserve(points.size());
        for (const ChartPoint& point : points) {
            xs.push_back(point.x);
            ys.push_back(point.y);
        }
        ImPlotSpec spec;
        spec.LineColor = color;
        spec.LineWeight = 1.0f;
        spec.Flags = static_cast<ImPlotItemFlags>(ImPlotItemFlags_NoLegend);
        ImPlot::PlotLine("##series", xs.data(), ys.data(), static_cast<int>(xs.size()), spec);
    }

    handle_interaction(sync, area);

    // The pixel positions of the data points, used by the tooltip.
    std::vector<SnapPoint> snap_points;
    snap_points.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        snap_points.push_back(SnapPoint{to_pixel_x(area, sync.x_min, sync.x_max, points[i].x),
                                        to_pixel_y(area, 0.0, y_range, points[i].y),
                                        static_cast<int>(i)});
    }
    m_snapper.set(std::move(snap_points));

    ImPlot::EndPlot();

    // The tooltip of the point under the cursor.
    if (area.hovered) {
        const ImVec2 mouse = ImGui::GetMousePos();
        if (const SnapPoint* snapped = m_snapper.find(mouse.x, mouse.y, kTooltipRadius)) {
            const ChartPoint& point = points[static_cast<std::size_t>(snapped->index)];
            std::vector<std::string> lines;
            if (tooltip_style == TooltipStyle::Difference) {
                lines.push_back(std::format("x: {:.3f}ms", point.x));
                lines.push_back(std::format("y: {} events", point.y));
            } else {
                lines.push_back(std::format("freq: {}Hz", std::lround(point.x)));
                lines.push_back(std::format("mag: {:.3f}", point.y));
            }
            show_tooltip(ImVec2(static_cast<float>(snapped->x), static_cast<float>(snapped->y)),
                         lines);
        }
    }

    ImGui::PopID();
}

void ChartRenderer::draw_diff_chart(const char* id,
                                    const char* title,
                                    const std::vector<double>& data,
                                    double y_max,
                                    ChartSync& sync) {
    // The histograms cover a one second window, expressed in milliseconds.
    sync.extent_min = 0.0;
    sync.extent_max = 1000.0;
    draw_line_chart(id, title, data, y_max, sync, kDiffColor, "ms", "events", 25.0,
                    TickStyle::Milliseconds, TooltipStyle::Difference);
}

void ChartRenderer::draw_freq_chart(const char* id,
                                    const char* title,
                                    const std::vector<double>& data,
                                    double y_max,
                                    double max_frequency,
                                    ChartSync& sync) {
    sync.extent_min = 0.0;
    sync.extent_max = max_frequency;
    draw_line_chart(id, title, data, y_max, sync, kFreqColor, "Hz", "magnitude", max_frequency,
                    TickStyle::Hertz, TooltipStyle::Frequency);
}

void ChartRenderer::reset_zoom() {
    m_timeline_sync = ChartSync{};
}

void ChartRenderer::set_timeline_range(double x_min, double x_max) {
    m_timeline_sync.initialized = true;
    m_timeline_sync.x_min = x_min;
    m_timeline_sync.x_max = x_max;
    // The range was set explicitly, it does not follow the end of a recording.
    m_timeline_sync.follow_end = false;
}

void ChartRenderer::draw_input_timeline(const char* id,
                                        const InputTimeline& timeline,
                                        const std::vector<std::string>& device_ids) {
    ImGui::PushID(id);

    const std::vector<std::string>& key_set = timeline.keys();
    const std::vector<Keypress>& keypresses = timeline.keypresses();
    const double key_count = static_cast<double>(key_set.size());

    // Map every device to a colour, in the order the devices were seen.
    std::map<std::string, ImU32> device_colors;
    const ImVec4* palette = device_palette();
    for (std::size_t i = 0; i < device_ids.size(); ++i)
        device_colors[device_ids[i]] = ImGui::ColorConvertFloat4ToU32(palette[i % 6]);

    // The index of every key on the y axis.
    std::map<std::string, double> key_indices;
    {
        double index = 0.0;
        for (const std::string& key : key_set)
            key_indices.emplace(key, index++);
    }

    // The keypresses that are drawn, they are the ones the tooltip snaps to.
    std::vector<const Keypress*> drawn;
    drawn.reserve(keypresses.size());

    for (const Keypress& keypress : keypresses) {
        if (key_indices.find(keypress.key) == key_indices.end())
            continue;
        drawn.push_back(&keypress);
    }

    const double end_timestamp = timeline.end_timestamp();

    ChartSync& sync = m_timeline_sync;
    const double previous_extent_max = sync.extent_max;
    sync.extent_min = 0.0;
    sync.extent_max = std::max(end_timestamp, 1.0);
    if (!sync.initialized) {
        sync.initialized = true;
        sync.x_min = sync.extent_min;
        sync.x_max = sync.extent_max;
        // A live recording follows its end.
        sync.follow_end = true;
    } else if (sync.follow_end && sync.x_max >= previous_extent_max - 1e-9) {
        // The view was at the right edge, follow the end of the recording.
        sync.x_max = sync.extent_max;
    }
    sync.x_max = std::clamp(sync.x_max, sync.extent_min, sync.extent_max);

    // The timeline takes up all the space that is left.
    if (!ImPlot::BeginPlot("##plot", ImVec2(-1.0f, -1.0f),
                            ImPlotFlags_NoTitle | ImPlotFlags_NoInputs)) {
        ImPlot::EndPlot();
        ImGui::PopID();
        return;
    }

    ImPlot::SetupAxis(ImAxis_X1, "s");
    ImPlot::SetupAxisLimits(ImAxis_X1, sync.x_min, sync.x_max, ImPlotCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, -0.5,
                            std::max(static_cast<double>(key_set.size()) - 0.5, 0.5),
                            ImPlotCond_Always);

    if (!key_set.empty()) {
        std::vector<double> tick_values;
        std::vector<const char*> tick_labels;
        tick_values.reserve(key_set.size());
        tick_labels.reserve(key_set.size());
        for (const std::string& key : key_set) {
            tick_values.push_back(static_cast<double>(tick_values.size()));
            tick_labels.push_back(key.c_str());
        }
        ImPlot::SetupAxisTicks(ImAxis_Y1, tick_values.data(), static_cast<int>(tick_values.size()),
                               tick_labels.data());
    }

    const PlotArea area = current_plot_area(sync.x_min, sync.x_max);

    handle_interaction(sync, area);

    // The pixel positions of the keypresses, used by the tooltip.
    std::vector<SnapPoint> snap_points;
    snap_points.reserve(drawn.size());
    for (std::size_t i = 0; i < drawn.size(); ++i) {
        const Keypress& keypress = *drawn[i];
        const double center = (keypress.start + keypress.end) / 2.0;
        const auto index = key_indices.find(keypress.key);
        snap_points.push_back(SnapPoint{to_pixel_x(area, sync.x_min, sync.x_max, center),
                                        to_pixel_y(area, -0.5, key_count - 0.5, index->second),
                                        static_cast<int>(i)});
    }
    m_snapper.set(std::move(snap_points));

    ImPlot::EndPlot();

    // The bars fill the row of their key, with a small margin on both sides.
    // They are drawn on the draw list, ImPlot's line items cannot be made
    // that thick.
    if (!drawn.empty()) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 plot_min = area.position;
        const ImVec2 plot_max(area.position.x + area.size.x, area.position.y + area.size.y);
        draw_list->PushClipRect(plot_min, plot_max, true);

        const double row_height = key_count > 0.0 ? area.size.y / key_count : 0.0;
        const double margin = std::min(kBarMargin, row_height / 4.0);

        for (const Keypress* keypress : drawn) {
            const double x0 = to_pixel_x(area, sync.x_min, sync.x_max, keypress->start);
            const double x1 = to_pixel_x(area, sync.x_min, sync.x_max, keypress->end);
            // Cull the bars that are outside of the view.
            if (x1 < plot_min.x || x0 > plot_max.x)
                continue;

            const auto index = key_indices.find(keypress->key);
            if (index == key_indices.end())
                continue;

            const double y_top = to_pixel_y(area, -0.5, key_count - 0.5, index->second + 0.5);
            const double y_bottom = to_pixel_y(area, -0.5, key_count - 0.5, index->second - 0.5);

            const ImVec2 p_min(static_cast<float>(x0), static_cast<float>(y_top + margin));
            const ImVec2 p_max(static_cast<float>(std::max(x1, x0 + 1.0)),
                               static_cast<float>(std::max(y_bottom - margin, y_top + margin + 1.0)));

            const ImU32 color = device_colors.count(keypress->device_id)
                                    ? device_colors[keypress->device_id]
                                    : IM_COL32(200, 200, 200, 255);
            draw_list->AddRectFilled(p_min, p_max, color);
        }

        draw_list->PopClipRect();
    }

    // The tooltip of the keypress under the cursor.
    if (area.hovered) {
        const ImVec2 mouse = ImGui::GetMousePos();
        if (const SnapPoint* snapped = m_snapper.find(mouse.x, mouse.y, kTooltipRadius)) {
            const Keypress& keypress = *drawn[static_cast<std::size_t>(snapped->index)];
            const double duration = std::max(0.0, keypress.end - keypress.start);
            show_tooltip(ImVec2(static_cast<float>(snapped->x), static_cast<float>(snapped->y)),
                         {std::format("device: {}", keypress.device_id),
                          std::format("key: {}", keypress.key),
                          std::format("start: {:.3f}s", keypress.start),
                          std::format("end: {:.3f}s", keypress.end),
                          std::format("duration: {:.3f}s", duration)});
        }
    }

    ImGui::PopID();
}

} // namespace kbi::ui
