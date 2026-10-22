#include "core/analyzer.h"
#include "core/decimate.h"
#include "core/fft.h"
#include "core/input_timeline.h"
#include "core/postprocess.h"
#include "parser/parser.h"
#include "parser/usb_descriptors.h"
#include "ui/charts.h"

#include <datetime.h>

#include <cmath>
#include <complex>
#include <bit>
#include <cstdio>
#include <format>
#include <print>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <memory>
#include <numbers>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what) {
    if (condition) {
        std::println("  ok   {}", what);
    } else {
        std::println("  FAIL {}", what);
        ++g_failures;
    }
}

// Reference DFT of the zero padded signal, computed directly. O(n^2) but only
// used for small inputs.
std::vector<std::complex<double>> naive_dft(const std::vector<double>& input, std::size_t size) {
    std::vector<std::complex<double>> result(size);
    for (std::size_t k = 0; k < size; ++k) {
        std::complex<double> sum(0.0, 0.0);
        for (std::size_t j = 0; j < input.size(); ++j) {
            const double angle = -2.0 * std::numbers::pi * static_cast<double>(k * j) /
                                 static_cast<double>(size);
            sum += input[j] * std::complex<double>(std::cos(angle), std::sin(angle));
        }
        result[k] = sum;
    }
    return result;
}

void test_fft() {
    std::println("fft");
    std::mt19937 rng(1234);

    for (const std::size_t n : {1u, 2u, 3u, 5u, 16u, 31u}) {
        std::vector<double> input(n);
        for (double& value : input)
            value = static_cast<double>(rng() % 1000) / 1000.0;

        const auto spectrum = kbi::fft::real_dft(input);
        const std::size_t m = spectrum.transform_size;
        check(m == std::bit_ceil(std::max<std::size_t>(n, 32)),
              std::format("the transform size is valid for n={}", n));
        check(spectrum.values.size() == (m / 2 + 1) * 2,
              std::format("the spectrum has one bin per frequency for n={}", n));

        // The reference is the dft of the zero padded signal.
        const auto reference = naive_dft(input, m);

        double worst = 0.0;
        for (std::size_t k = 0; k < m / 2 + 1; ++k) {
            const std::complex<double> got(spectrum.values[k * 2], spectrum.values[k * 2 + 1]);
            worst = std::max(worst, std::abs(got - reference[k]));
        }
        // PFFFT works in single precision.
        check(worst < 1e-4,
              std::format("the spectrum matches the reference dft for n={} (err={})", n, worst));
    }

    // A pure tone must show up as a single bin.
    const std::size_t m = 16384;
    std::vector<double> tone(m);
    for (std::size_t i = 0; i < m; ++i) {
        tone[i] = std::cos(2.0 * std::numbers::pi * 100.0 * static_cast<double>(i) /
                           static_cast<double>(m));
    }
    const auto spectrum = kbi::fft::real_dft(tone);
    std::size_t peak = 0;
    double peak_value = 0.0;
    for (std::size_t k = 0; k < spectrum.values.size() / 2; ++k) {
        const double magnitude = std::hypot(spectrum.values[k * 2], spectrum.values[k * 2 + 1]);
        if (magnitude > peak_value) {
            peak_value = magnitude;
            peak = k;
        }
    }
    check(peak == 100, std::format("a 100Hz tone peaks at bin 100 (got {})", peak));

    // The transform is not padded when PFFFT supports the size, the bin rate
    // is 125 * 2^n.
    const std::vector<double> bin_rate(16000, 1.0);
    check(kbi::fft::real_dft(bin_rate).transform_size == 16000,
          "the transform of a bin rate sized signal is not padded");

    // A size that PFFFT does not support is rounded up to a power of two.
    const std::vector<double> small(1000, 1.0);
    check(kbi::fft::real_dft(small).transform_size == 1024,
          "an unsupported size is rounded up");
    check(std::abs(peak_value - static_cast<double>(m) / 2.0) < 1.0,
          "the magnitude of the tone is m/2");
}

void test_analyzer() {
    std::println("analyzer");

    kbi::Analyzer analyzer;
    const int bin_rate = analyzer.bin_rate();

    // A perfectly periodic event train: every 10ms. The first event is not
    // at zero, otherwise the web analyzer skips its diff as well.
    std::vector<double> timestamps;
    for (int i = 0; i < 200; ++i)
        timestamps.push_back(1000.0 + static_cast<double>(i) * 10000.0);
    analyzer.add(timestamps);

    check(analyzer.event_count() == 200, "event count");

    const double interval = 1000000.0 / bin_rate;
    const auto index = static_cast<std::size_t>(std::llround(10000.0 / interval));
    check(analyzer.consecutive_diff()[index] == 199.0,
          "consecutive diff histogram counts the 10ms gaps");
    check(analyzer.consecutive_diff_max() == 199.0, "consecutive diff max");

    // A 10ms event train is a 100Hz rhythm, the spectrum has to peak at
    // 100Hz. The transform runs at the bin rate, so the frequency of a bin is
    // its index in hertz.
    {
        std::shared_ptr<const kbi::FreqSnapshot> snapshot;
        for (int i = 0; i < 1000 && !snapshot; ++i) {
            snapshot = analyzer.freq_snapshot();
            if (!snapshot)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        check(snapshot != nullptr, "the frequency snapshot is ready");
        if (snapshot) {
            // The differences to all the previous events form a comb with a
            // period of 10ms, its spectrum peaks at 100Hz.
            std::size_t peak = 0;
            double peak_value = 0.0;
            // Skip the direct current component.
            for (std::size_t k = 1; k < snapshot->all_diff.size(); ++k) {
                if (snapshot->all_diff[k] > peak_value) {
                    peak_value = snapshot->all_diff[k];
                    peak = k;
                }
            }
            check(peak == 100, std::format("the 100Hz rhythm peaks at 100Hz (got {})", peak));
        }
    }

    // The spectrum covers every frequency from 0 to bin_rate / 2 Hz, which is
    // bin_rate / 2 + 1 entries. The transform runs at the bin rate, so the
    // frequency of a bin is its index in hertz.
    for (const int rate : {1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000}) {
        kbi::Analyzer rate_analyzer;
        rate_analyzer.set_bin_rate(rate);
        std::vector<double> stamps;
        for (int i = 0; i < 100; ++i)
            stamps.push_back(static_cast<double>(i) * 1000000.0 / rate);
        rate_analyzer.add(stamps);

        std::shared_ptr<const kbi::FreqSnapshot> snapshot;
        for (int i = 0; i < 1000 && !snapshot; ++i) {
            snapshot = rate_analyzer.freq_snapshot();
            if (!snapshot)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        check(snapshot != nullptr,
              std::format("the frequency snapshot of a {}Hz bin rate is ready", rate));
        if (snapshot) {
            // PFFFT needs a transform size that is a multiple of 32, the two
            // small bin rates are rounded up to the next power of two and
            // their spectrum has a few entries above the axis.
            const std::size_t transform_size =
                rate % 32 == 0 ? static_cast<std::size_t>(rate)
                               : std::bit_ceil(static_cast<unsigned>(rate));
            check(snapshot->consecutive_diff.size() == transform_size / 2 + 1,
                  std::format("a {}Hz bin rate has {} frequency entries", rate,
                              transform_size / 2 + 1));
        }
    }

    // Non monotonic timestamps are ignored.
    analyzer.add({1000});
    check(analyzer.event_count() == 200, "non monotonic timestamps are ignored");

    // Changing the bin rate re-ingests the raw timestamps.
    analyzer.set_bin_rate(8000);
    check(analyzer.bin_rate() == 8000, "bin rate changed");
    check(analyzer.event_count() == 200, "event count after the bin rate change");
    const double new_interval = 1000000.0 / 8000.0;
    const auto new_index = static_cast<std::size_t>(std::llround(10000.0 / new_interval));
    check(analyzer.consecutive_diff()[new_index] == 199.0,
          "consecutive diff histogram after the bin rate change");

    // Invalid bin rates are rejected.
    analyzer.set_bin_rate(999);
    check(analyzer.bin_rate() == 8000, "bin rate that is not a multiple of 125 is rejected");
    analyzer.set_bin_rate(1500);
    check(analyzer.bin_rate() == 8000, "bin rate that is not 125 * 2^n is rejected");
    analyzer.set_bin_rate(-1);
    check(analyzer.bin_rate() == 8000, "negative bin rate is rejected");
    analyzer.set_bin_rate(1000);
    check(analyzer.bin_rate() == 1000, "valid bin rate accepted");
}

void test_decimate() {
    std::println("decimate");

    std::vector<double> data(1001);
    for (std::size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<double>(i % 7);

    const auto points = kbi::decimate(data, 0.0, 1000.0, 0.0, 1000.0, 100);
    check(points.size() <= 100, "at most one point per pixel column");
    check(!points.empty(), "points are produced");

    // The maximum of the whole range must be preserved by the bucket max.
    double expected_max = 0.0;
    for (const double value : data)
        expected_max = std::max(expected_max, value);
    double actual_max = 0.0;
    for (const auto& point : points)
        actual_max = std::max(actual_max, point.y);
    check(actual_max == expected_max, "the bucket max preserves the global maximum");

    // Zooming in must not produce points outside the visible domain.
    const auto zoomed = kbi::decimate(data, 0.0, 1000.0, 500.0, 510.0, 100);
    for (const auto& point : zoomed)
        check(point.x >= 490.0 && point.x <= 520.0, "zoomed points stay in range");
}

void test_postprocess() {
    std::println("postprocess");
    std::vector<double> data(100, 1.0);
    const auto with_low_cut = kbi::postprocess_with_max(data, {true});
    check(with_low_cut.data[0] < 0.01, "the low cut removes the dc component");
    check(with_low_cut.data[99] == 1.0, "the low cut leaves the high bins alone");
    check(with_low_cut.max == 1.0, "the max is computed");

    const auto without_low_cut = kbi::postprocess_with_max(data, {false});
    check(without_low_cut.data[0] == 1.0, "the low cut can be disabled");
}

void test_timeline() {
    std::println("input timeline");
    kbi::InputTimeline timeline;
    timeline.add("kb", {1000, true, 4});
    timeline.add("kb", {6000, false, 4});
    check(timeline.keypresses().size() == 1, "one keypress is recorded");
    check(timeline.keypresses()[0].start == 0.001, "the keypress starts at the press");
    check(timeline.keypresses()[0].end == 0.006, "the keypress ends at the release");
    check(timeline.keypresses()[0].key == "A", "the keypress is named");
    check(timeline.keys().size() == 1, "the key set is updated");

    // A release without a matching press is ignored.
    timeline.add("kb", {7000, false, 5});
    check(timeline.keypresses().size() == 1, "unmatched releases are ignored");
}

void test_base64() {
    std::println("usb descriptors");
    // "Hello" -> "SGVsbG8="
    const std::string decoded = kbi::base64_decode("SGVsbG8=");
    check(decoded == "Hello", "base64 decoding");

    // A device descriptor followed by a HID descriptor.
    const std::vector<unsigned char> raw = {
        0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x31, 0x31, 0x50, 0x27, 0x00,
        0x02, 0x01, 0x01, 0x03, 0x21,
        0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x3f, 0x00};
    const auto descriptors = kbi::parse_descriptors(raw);
    check(descriptors.size() == 2, "the descriptor blob is split");
    check(descriptors[0].type_name == "DEVICE", "the device descriptor is recognized");
    check(descriptors[1].type_name == "HID", "the hid descriptor is recognized");
    check(descriptors[0].fields.size() == 14, "the device descriptor fields are decoded");
    check(descriptors[0].fields[7].value == "0x3131", "idVendor is decoded");
}

void test_chart_interaction() {
    std::println("chart interaction");

    kbi::ui::ChartSync sync;
    sync.extent_min = 0.0;
    sync.extent_max = 1000.0;
    sync.x_min = 100.0;
    sync.x_max = 125.0;

    // Dragging to the right moves the view to the left.
    kbi::ui::ChartInteraction drag;
    drag.dragging = true;
    drag.drag_delta_x = 50.0f;
    drag.plot_width = 500.0f;
    kbi::ui::update_range(sync, drag);
    check(std::abs(sync.x_min - 97.5) < 1e-9, "dragging pans the view");
    check(std::abs(sync.x_max - 122.5) < 1e-9, "dragging keeps the span");

    // The view cannot leave the data extent.
    kbi::ui::ChartInteraction big_drag;
    big_drag.dragging = true;
    big_drag.drag_delta_x = -10000.0f;
    big_drag.plot_width = 500.0f;
    kbi::ui::update_range(sync, big_drag);
    check(sync.x_max <= sync.extent_max + 1e-9, "the view is clamped to the extent");
    check(sync.x_max - sync.x_min > 0.0, "the view keeps a positive span");

    // Scrolling zooms around the cursor.
    kbi::ui::ChartSync zoom_sync;
    zoom_sync.extent_min = 0.0;
    zoom_sync.extent_max = 1000.0;
    zoom_sync.x_min = 0.0;
    zoom_sync.x_max = 100.0;
    kbi::ui::ChartInteraction zoom;
    zoom.wheel = 1.0f;
    zoom.mouse_x = 50.0;
    kbi::ui::update_range(zoom_sync, zoom);
    check(zoom_sync.x_max - zoom_sync.x_min < 100.0, "scrolling zooms in");
    check(std::abs((zoom_sync.x_min + zoom_sync.x_max) * 0.5 - 50.0) < 1e-6,
          "zooming keeps the cursor position");

    kbi::ui::ChartInteraction zoom_out;
    zoom_out.wheel = -1.0f;
    zoom_out.mouse_x = 50.0;
    kbi::ui::update_range(zoom_sync, zoom_out);
    check(zoom_sync.x_max - zoom_sync.x_min > 90.0, "scrolling down zooms out");

    // Zooming out never goes beyond the extent.
    for (int i = 0; i < 100; ++i) {
        kbi::ui::ChartInteraction step;
        step.wheel = -1.0f;
        step.mouse_x = 500.0;
        kbi::ui::update_range(zoom_sync, step);
    }
    check(std::abs(zoom_sync.x_min - 0.0) < 1e-9 && std::abs(zoom_sync.x_max - 1000.0) < 1e-9,
          "zooming out stops at the extent");

    // A double click resets the view.
    kbi::ui::ChartInteraction reset;
    reset.double_clicked = true;
    kbi::ui::update_range(zoom_sync, reset);
    check(zoom_sync.x_min == 0.0 && zoom_sync.x_max == 1000.0, "a double click resets the view");
}

void test_usb_polling_rate() {
    std::println("usb polling rate");

    // A device descriptor (USB 2.0) followed by an interrupt endpoint with a
    // bInterval of 10 and a control endpoint with a bInterval of 0.
    const std::vector<unsigned char> raw = {
        0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x31, 0x31, 0x50, 0x27, 0x00,
        0x02, 0x01, 0x01, 0x03, 0x21,
        0x09, 0x07, 0x00, 0x02, 0x00, 0x00, 0x0a, 0x00, 0x00,
        0x07, 0x05, 0x81, 0x03, 0x08, 0x00, 0x0a,
        // A control endpoint, its interval is zero.
        0x07, 0x05, 0x80, 0x02, 0x08, 0x00, 0x00,
    };

    const auto descriptors = kbi::parse_usb_descriptors(raw);
    check(descriptors.size() == 4, "the descriptor blob is split");
    if (descriptors.size() < 4)
        return;

    // The USB version is added to the device descriptor.
    check(descriptors[0].fields[2].value == "0200 (USB 2.0)", "the usb version is annotated");

    const auto& interval = descriptors[2].fields[5];
    check(interval.name == "bInterval", "the endpoint interval is found");
    check(interval.children.size() == 3, "the polling rate is calculated");
    if (interval.children.size() == 3) {
        check(interval.children[0].value == "1.25ms", "the requested interval is formatted");
        check(interval.children[1].value == "1ms", "the effective interval is formatted");
        check(interval.children[2].value == "1000Hz", "the polling rate is calculated");
    }

    // A control endpoint has an interval of zero, which must not divide by
    // zero.
    const auto& control_interval = descriptors[3].fields[5];
    check(control_interval.children.size() == 3, "the control endpoint is decoded");
    if (control_interval.children.size() == 3)
        check(control_interval.children[2].value == "InfinityHz",
              "an interval of zero does not divide by zero");
}

void test_point_snapper() {
    std::println("point snapper");

    kbi::ui::PointSnapper snapper;
    check(snapper.find(0.0, 0.0, 40.0) == nullptr, "an empty snapper finds nothing");

    // The points are not sorted, the snapper sorts them.
    snapper.set({{30.0, 5.0, 2}, {10.0, 5.0, 0}, {20.0, 5.0, 1}});

    const kbi::ui::SnapPoint* snapped = snapper.find(11.0, 5.0, 40.0);
    check(snapped != nullptr, "a point is found");
    if (snapped) {
        check(snapped->index == 0, "the closest point is found");
        check(snapped->x == 10.0, "the position of the point is kept");
    }

    // A point that is too far away is not found.
    check(snapper.find(500.0, 500.0, 40.0) == nullptr, "a point outside of the radius is not found");

    // The closest point wins, not the first one.
    snapper.set({{10.0, 100.0, 0}, {12.0, 0.0, 1}});
    snapped = snapper.find(11.0, 0.0, 40.0);
    check(snapped != nullptr && snapped->index == 1, "the closest point wins");

    // The radius is measured in pixels.
    snapper.set({{10.0, 0.0, 0}});
    check(snapper.find(10.0, 39.0, 40.0) != nullptr, "a point inside the radius is found");
    check(snapper.find(10.0, 41.0, 40.0) == nullptr, "a point outside the radius is not found");
}

void test_parser(const std::string& path) {
    std::println("parser");
    const auto result = kbi::parse_result_file(path);
    check(result.has_value(), "the recording is parsed");
    if (!result)
        return;
    check(result->inputs.size() == 2, "both devices are present");
    check(result->devices.size() == 2, "the device list is parsed");
    check(result->usb_devices.size() == 1, "the usb device list is parsed");
    check(result->total_input_count() == 1240, "all input events are parsed");
    check(result->info.backend == "evdev", "the metadata is parsed");
}

void test_datetime() {
    std::println("datetime");

    // The .NET epoch is 0001-01-01, the unix epoch is 1970-01-01.
    const auto unix_epoch = std::chrono::system_clock::time_point{};
    check(kbi::format_iso8601_utc(unix_epoch) == "1970-01-01T00:00:00Z",
          "the unix epoch is formatted correctly");

    // 2026-09-26T18:39:27.211Z, the time of the "SayoDevice 7K mechanical.kbi"
    // recording.
    const std::uint64_t ticks = 639'260'447'672'114'184;
    const auto time_point = kbi::from_net_datetime_ticks(ticks);
    check(kbi::format_iso8601_utc(time_point, true) == "2026-09-26T18:39:27.211Z",
          "the .NET ticks are converted correctly (" + kbi::format_iso8601_utc(time_point, true) +
              ")");

    // The two upper bits hold the DateTimeKind, they are not part of the time.
    const std::uint64_t with_kind = ticks | (2ULL << 62);
    check(kbi::from_net_datetime_ticks(with_kind) == time_point,
          "the DateTimeKind bits are ignored");
    check(kbi::from_net_datetime_ticks(ticks | (1ULL << 62)) == time_point,
          "the Utc kind is ignored");

    // The round trip through the .NET DateTime encoding, the same conversion
    // the .kbi exporter uses.
    const auto round_trip = kbi::from_net_datetime_ticks(
        kbi::to_net_datetime_ticks(time_point, kbi::kNetDateTimeKindUtc));
    check(round_trip == time_point, "the .NET DateTime encoding round trips");

    // The kind bits are set, but they do not change the time.
    check(kbi::to_net_datetime_ticks(time_point, kbi::kNetDateTimeKindUtc) !=
              kbi::to_net_datetime_ticks(time_point, kbi::kNetDateTimeKindLocal),
          "the DateTimeKind is stored in the upper bits");
    check((kbi::to_net_datetime_ticks(time_point, kbi::kNetDateTimeKindUtc) >> 62) == 1,
          "the Utc kind is written");

    // A time before the unix epoch.
    const auto before = kbi::from_net_datetime_ticks(621'355'968'000'000'000ULL - 1);
    check(kbi::format_iso8601_utc(before) == "1969-12-31T23:59:59Z",
          "times before the unix epoch are converted correctly (" +
              kbi::format_iso8601_utc(before) + ")");
}

void test_legacy_parser(const std::string& path) {
    std::println("legacy kbi parser");
    const auto result = kbi::parse_result_file(path);
    check(result.has_value(), "the legacy recording is parsed");
    if (!result)
        return;
    check(result->inputs.size() == 2, "both sources are present");
    check(result->devices.size() == 2, "the device list is parsed");
    check(result->devices.at("HID_VID_3149&PID_5041&MI_00").name == "MonsGeek Keyboard",
          "the device name is parsed");
    check(result->devices.at("HID_VID_3149&PID_5041&MI_00").vid == 0x3149,
          "the vendor id is scraped from the device id");
    check(result->devices.at("HID_VID_3149&PID_5041&MI_00").pid == 0x5041,
          "the product id is scraped from the device id");
    check(result->total_input_count() == 8, "all events are parsed");
    check(result->time == "2025-07-22T22:34:43.000Z",
          "the recording time is converted correctly (" + result->time + ")");

    const auto& keyboard = result->inputs.at("HID_VID_3149&PID_5041&MI_00");
    check(keyboard.size() == 4, "the keyboard events are grouped by source");
    check(keyboard[0].timestamp == 0, "the first timestamp is parsed");
    check(keyboard[0].code == 0x04, "the key name is mapped to a keycode");
    check(keyboard[1].pressed == false, "the release event is parsed");
    check(keyboard[2].code == 0x1E, "the digit key name is mapped to a keycode");
    check(result->inputs.at("HID_VID_3149&PID_5042&MI_00")[1].code == 0x3E,
          "the function key name is mapped to a keycode");
}

} // namespace

// Dumps the analysis of a recording so it can be compared against a reference
// implementation of the web analyzer.
static int dump_analysis(const std::string& path, const std::string& output_path) {
    const auto result = kbi::parse_result_file(path);
    if (!result) {
        std::println(stderr, "Could not parse {}", path);
        return 1;
    }

    std::vector<double> timestamps;
    for (const auto& [id, events] : result->inputs)
        for (const kbi::InputEvent& event : events)
            timestamps.push_back(event.timestamp);
    std::sort(timestamps.begin(), timestamps.end());

    kbi::Analyzer analyzer;
    analyzer.add(timestamps);

    // Wait for the worker thread to produce the frequency domain snapshot.
    std::shared_ptr<const kbi::FreqSnapshot> snapshot;
    for (int i = 0; i < 1000 && !snapshot; ++i) {
        snapshot = analyzer.freq_snapshot();
        if (!snapshot)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!snapshot) {
        std::println(stderr, "The frequency snapshot was not ready");
        return 1;
    }

    std::ofstream out(output_path);
    out.precision(17);
    if (!out) {
        std::println(stderr, "Could not write {}", output_path);
        return 1;
    }

    const auto dump = [&out](const char* name, const std::vector<double>& values) {
        out << name << " " << values.size();
        for (const double value : values)
            out << " " << value;
        out << "\n";
    };

    dump("consecutive_diff", analyzer.consecutive_diff());
    dump("all_diff", analyzer.all_diff());
    dump("wrapped_timestamp", analyzer.wrapped_timestamp());
    dump("consecutive_diff_freq", snapshot->consecutive_diff);
    dump("all_diff_freq", snapshot->all_diff);
    dump("wrapped_timestamp_freq", snapshot->wrapped_timestamp);
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 3 && std::string(argv[1]) == "--dump")
        return dump_analysis(argv[2], argv[3]);

    test_fft();
    test_analyzer();
    test_decimate();
    test_postprocess();
    test_timeline();
    test_base64();
    test_datetime();
    test_usb_polling_rate();
    test_chart_interaction();
    test_point_snapper();
    if (argc > 1)
        test_parser(argv[1]);
    if (argc > 2)
        test_legacy_parser(argv[2]);

    if (g_failures == 0) {
        std::println("\nAll tests passed.");
        return 0;
    }
    std::println("\n{} test(s) failed.", g_failures);
    return 1;
}
