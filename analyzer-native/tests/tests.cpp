#include "core/analyzer.h"
#include "core/decimate.h"
#include "core/fft.h"
#include "core/input_timeline.h"
#include "core/postprocess.h"
#include "parser/parser.h"
#include "parser/usb_descriptors.h"
#include "ui/charts.h"

#include <datetime.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <format>
#include <fstream>
#include <memory>
#include <numbers>
#include <print>
#include <random>
#include <string>
#include <thread>
#include <vector>

namespace {

int gFailures = 0;

void check(bool condition, const std::string& what)
{
    if (condition) {
        std::println("  ok   {}", what);
    } else {
        std::println("  FAIL {}", what);
        ++gFailures;
    }
}

// Reference DFT of the zero padded signal, computed directly. O(n^2) but only
// used for small inputs.
std::vector<std::complex<double>> naiveDft(const std::vector<double>& input, std::size_t size)
{
    std::vector<std::complex<double>> result(size);
    for (std::size_t k = 0; k < size; ++k) {
        std::complex<double> sum(0.0, 0.0);
        for (std::size_t j = 0; j < input.size(); ++j) {
            const double angle = -2.0 * std::numbers::pi * static_cast<double>(k * j) / static_cast<double>(size);
            sum += input[j] * std::complex<double>(std::cos(angle), std::sin(angle));
        }
        result[k] = sum;
    }
    return result;
}

void testFft()
{
    std::println("fft");
    // A fixed seed keeps the test deterministic.
    std::mt19937 rng(1234); // NOLINT(bugprone-random-generator-seed)

    for (const std::size_t n : { 1u, 2u, 3u, 5u, 16u, 31u }) {
        std::vector<double> input(n);
        for (double& value : input)
            value = static_cast<double>(rng() % 1000) / 1000.0;

        const auto spectrum = Kbi::Fft::realDft(input);
        const std::size_t m = spectrum.transformSize;
        check(m == std::bit_ceil(std::max<std::size_t>(n, 32)),
            std::format("the transform size is valid for n={}", n));
        check(spectrum.values.size() == (m / 2 + 1) * 2,
            std::format("the spectrum has one bin per frequency for n={}", n));

        // The reference is the dft of the zero padded signal.
        const auto reference = naiveDft(input, m);

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
        tone[i] = std::cos(2.0 * std::numbers::pi * 100.0 * static_cast<double>(i) / static_cast<double>(m));
    }
    const auto spectrum = Kbi::Fft::realDft(tone);
    std::size_t peak = 0;
    double peakValue = 0.0;
    for (std::size_t k = 0; k < spectrum.values.size() / 2; ++k) {
        const double magnitude = std::hypot(spectrum.values[k * 2], spectrum.values[k * 2 + 1]);
        if (magnitude > peakValue) {
            peakValue = magnitude;
            peak = k;
        }
    }
    check(peak == 100, std::format("a 100Hz tone peaks at bin 100 (got {})", peak));

    // The transform is not padded when PFFFT supports the size, the bin rate
    // is 125 * 2^n.
    const std::vector<double> binRate(16000, 1.0);
    check(Kbi::Fft::realDft(binRate).transformSize == 16000,
        "the transform of a bin rate sized signal is not padded");

    // A size that PFFFT does not support is rounded up to a power of two.
    const std::vector<double> small(1000, 1.0);
    check(Kbi::Fft::realDft(small).transformSize == 1024,
        "an unsupported size is rounded up");
    check(std::abs(peakValue - static_cast<double>(m) / 2.0) < 1.0,
        "the magnitude of the tone is m/2");
}

void testAnalyzer()
{
    std::println("analyzer");

    Kbi::Analyzer analyzer;
    const int binRate = analyzer.binRate();

    // A perfectly periodic event train: every 10ms. The first event is not
    // at zero, otherwise the web analyzer skips its diff as well.
    std::vector<double> timestamps;
    timestamps.reserve(200);
    for (int i = 0; i < 200; ++i)
        timestamps.push_back(1000.0 + static_cast<double>(i) * 10000.0);
    analyzer.add(timestamps);

    check(analyzer.eventCount() == 200, "event count");

    const double interval = 1000000.0 / binRate;
    const auto index = static_cast<std::size_t>(std::llround(10000.0 / interval));
    check(analyzer.consecutiveDiff()[index] == 199.0,
        "consecutive diff histogram counts the 10ms gaps");
    check(analyzer.consecutiveDiffMax() == 199.0, "consecutive diff max");

    // A 10ms event train is a 100Hz rhythm, the spectrum has to peak at
    // 100Hz. The transform runs at the bin rate, so the frequency of a bin is
    // its index in hertz.
    {
        std::shared_ptr<const Kbi::FreqSnapshot> snapshot;
        for (int i = 0; i < 1000 && !snapshot; ++i) {
            snapshot = analyzer.freqSnapshot();
            if (!snapshot)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        check(snapshot != nullptr, "the frequency snapshot is ready");
        if (snapshot) {
            // The differences to all the previous events form a comb with a
            // period of 10ms, its spectrum peaks at 100Hz.
            std::size_t peak = 0;
            double peakValue = 0.0;
            // Skip the direct current component.
            for (std::size_t k = 1; k < snapshot->allDiff.size(); ++k) {
                if (snapshot->allDiff[k] > peakValue) {
                    peakValue = snapshot->allDiff[k];
                    peak = k;
                }
            }
            check(peak == 100, std::format("the 100Hz rhythm peaks at 100Hz (got {})", peak));
        }
    }

    // The spectrum covers every frequency from 0 to bin_rate / 2 Hz, which is
    // bin_rate / 2 + 1 entries. The transform runs at the bin rate, so the
    // frequency of a bin is its index in hertz.
    for (const int rate : { 1000, 2000, 4000, 8000, 16000, 32000, 64000, 128000 }) {
        Kbi::Analyzer rateAnalyzer;
        rateAnalyzer.setBinRate(rate);
        std::vector<double> stamps;
        stamps.reserve(100);
        for (int i = 0; i < 100; ++i)
            stamps.push_back(static_cast<double>(i) * 1000000.0 / rate);
        rateAnalyzer.add(stamps);

        std::shared_ptr<const Kbi::FreqSnapshot> snapshot;
        for (int i = 0; i < 1000 && !snapshot; ++i) {
            snapshot = rateAnalyzer.freqSnapshot();
            if (!snapshot)
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        check(snapshot != nullptr,
            std::format("the frequency snapshot of a {}Hz bin rate is ready", rate));
        if (snapshot) {
            // PFFFT needs a transform size that is a multiple of 32, the two
            // small bin rates are rounded up to the next power of two and
            // their spectrum has a few entries above the axis.
            const std::size_t transformSize = rate % 32 == 0 ? static_cast<std::size_t>(rate)
                                                             : std::bit_ceil(static_cast<unsigned>(rate));
            check(snapshot->consecutiveDiff.size() == transformSize / 2 + 1,
                std::format("a {}Hz bin rate has {} frequency entries", rate,
                    transformSize / 2 + 1));
        }
    }

    // Non monotonic timestamps are ignored.
    analyzer.add({ 1000 });
    check(analyzer.eventCount() == 200, "non monotonic timestamps are ignored");

    // Changing the bin rate re-ingests the raw timestamps.
    analyzer.setBinRate(8000);
    check(analyzer.binRate() == 8000, "bin rate changed");
    check(analyzer.eventCount() == 200, "event count after the bin rate change");
    const double newInterval = 1000000.0 / 8000.0;
    const auto newIndex = static_cast<std::size_t>(std::llround(10000.0 / newInterval));
    check(analyzer.consecutiveDiff()[newIndex] == 199.0,
        "consecutive diff histogram after the bin rate change");

    // Invalid bin rates are rejected.
    analyzer.setBinRate(999);
    check(analyzer.binRate() == 8000, "bin rate that is not a multiple of 125 is rejected");
    analyzer.setBinRate(1500);
    check(analyzer.binRate() == 8000, "bin rate that is not 125 * 2^n is rejected");
    analyzer.setBinRate(-1);
    check(analyzer.binRate() == 8000, "negative bin rate is rejected");
    analyzer.setBinRate(1000);
    check(analyzer.binRate() == 1000, "valid bin rate accepted");
}

void testDecimate()
{
    std::println("decimate");

    std::vector<double> data(1001);
    for (std::size_t i = 0; i < data.size(); ++i)
        data[i] = static_cast<double>(i % 7);

    const auto points = Kbi::decimate(data, 0.0, 1000.0, 0.0, 1000.0, 100);
    check(points.size() <= 100, "at most one point per pixel column");
    check(!points.empty(), "points are produced");

    // The maximum of the whole range must be preserved by the bucket max.
    double expectedMax = 0.0;
    for (const double value : data)
        expectedMax = std::max(expectedMax, value);
    double actualMax = 0.0;
    for (const auto& point : points)
        actualMax = std::max(actualMax, point.y);
    check(actualMax == expectedMax, "the bucket max preserves the global maximum");

    // Zooming in must not produce points outside the visible domain.
    const auto zoomed = Kbi::decimate(data, 0.0, 1000.0, 500.0, 510.0, 100);
    for (const auto& point : zoomed)
        check(point.x >= 490.0 && point.x <= 520.0, "zoomed points stay in range");
}

void testPostprocess()
{
    std::println("postprocess");
    std::vector<double> data(100, 1.0);
    const auto withLowCut = Kbi::postprocessWithMax(data, { true });
    check(withLowCut.data[0] < 0.01, "the low cut removes the dc component");
    check(withLowCut.data[99] == 1.0, "the low cut leaves the high bins alone");
    check(withLowCut.max == 1.0, "the max is computed");

    const auto withoutLowCut = Kbi::postprocessWithMax(data, { false });
    check(withoutLowCut.data[0] == 1.0, "the low cut can be disabled");
}

void testTimeline()
{
    std::println("input timeline");
    Kbi::InputTimeline timeline;
    timeline.add("kb", { .timestamp = 1000, .pressed = true, .code = 4 });
    timeline.add("kb", { .timestamp = 6000, .pressed = false, .code = 4 });
    check(timeline.keypresses().size() == 1, "one keypress is recorded");
    check(timeline.keypresses()[0].start == 0.001, "the keypress starts at the press");
    check(timeline.keypresses()[0].end == 0.006, "the keypress ends at the release");
    check(timeline.keypresses()[0].key == "A", "the keypress is named");
    check(timeline.keys().size() == 1, "the key set is updated");

    // A release without a matching press is ignored.
    timeline.add("kb", { .timestamp = 7000, .pressed = false, .code = 5 });
    check(timeline.keypresses().size() == 1, "unmatched releases are ignored");
}

void testBase64()
{
    std::println("usb descriptors");
    // "Hello" -> "SGVsbG8="
    const std::string decoded = Kbi::base64Decode("SGVsbG8=");
    check(decoded == "Hello", "base64 decoding");

    // A device descriptor followed by a HID descriptor.
    const std::vector<unsigned char> raw = {
        0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x31, 0x31, 0x50, 0x27, 0x00,
        0x02, 0x01, 0x01, 0x03, 0x21,
        0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, 0x3f, 0x00
    };
    const auto descriptors = Kbi::parseDescriptors(raw);
    check(descriptors.size() == 2, "the descriptor blob is split");
    check(descriptors[0].typeName == "DEVICE", "the device descriptor is recognized");
    check(descriptors[1].typeName == "HID", "the hid descriptor is recognized");
    check(descriptors[0].fields.size() == 14, "the device descriptor fields are decoded");
    check(descriptors[0].fields[7].value == "0x3131", "idVendor is decoded");
}

void testChartInteraction()
{
    std::println("chart interaction");

    Kbi::Ui::ChartSync sync;
    sync.extentMin = 0.0;
    sync.extentMax = 1000.0;
    sync.xMin = 100.0;
    sync.xMax = 125.0;

    // Dragging to the right moves the view to the left.
    Kbi::Ui::ChartInteraction drag;
    drag.dragging = true;
    drag.dragDeltaX = 50.0f;
    drag.plotWidth = 500.0f;
    Kbi::Ui::updateRange(sync, drag);
    check(std::abs(sync.xMin - 97.5) < 1e-9, "dragging pans the view");
    check(std::abs(sync.xMax - 122.5) < 1e-9, "dragging keeps the span");

    // The view cannot leave the data extent.
    Kbi::Ui::ChartInteraction bigDrag;
    bigDrag.dragging = true;
    bigDrag.dragDeltaX = -10000.0f;
    bigDrag.plotWidth = 500.0f;
    Kbi::Ui::updateRange(sync, bigDrag);
    check(sync.xMax <= sync.extentMax + 1e-9, "the view is clamped to the extent");
    check(sync.xMax - sync.xMin > 0.0, "the view keeps a positive span");

    // Scrolling zooms around the cursor.
    Kbi::Ui::ChartSync zoomSync;
    zoomSync.extentMin = 0.0;
    zoomSync.extentMax = 1000.0;
    zoomSync.xMin = 0.0;
    zoomSync.xMax = 100.0;
    Kbi::Ui::ChartInteraction zoom;
    zoom.wheel = 1.0f;
    zoom.mouseX = 50.0;
    Kbi::Ui::updateRange(zoomSync, zoom);
    check(zoomSync.xMax - zoomSync.xMin < 100.0, "scrolling zooms in");
    check(std::abs((zoomSync.xMin + zoomSync.xMax) * 0.5 - 50.0) < 1e-6,
        "zooming keeps the cursor position");

    Kbi::Ui::ChartInteraction zoomOut;
    zoomOut.wheel = -1.0f;
    zoomOut.mouseX = 50.0;
    Kbi::Ui::updateRange(zoomSync, zoomOut);
    check(zoomSync.xMax - zoomSync.xMin > 90.0, "scrolling down zooms out");

    // Zooming out never goes beyond the extent.
    for (int i = 0; i < 100; ++i) {
        Kbi::Ui::ChartInteraction step;
        step.wheel = -1.0f;
        step.mouseX = 500.0;
        Kbi::Ui::updateRange(zoomSync, step);
    }
    check(std::abs(zoomSync.xMin - 0.0) < 1e-9 && std::abs(zoomSync.xMax - 1000.0) < 1e-9,
        "zooming out stops at the extent");

    // A double click resets the view.
    Kbi::Ui::ChartInteraction reset;
    reset.doubleClicked = true;
    Kbi::Ui::updateRange(zoomSync, reset);
    check(zoomSync.xMin == 0.0 && zoomSync.xMax == 1000.0, "a double click resets the view");
}

void testUsbPollingRate()
{
    std::println("usb polling rate");

    // A device descriptor (USB 2.0) followed by an interrupt endpoint with a
    // bInterval of 10 and a control endpoint with a bInterval of 0.
    const std::vector<unsigned char> raw = {
        0x12,
        0x01,
        0x00,
        0x02,
        0x00,
        0x00,
        0x00,
        0x40,
        0x31,
        0x31,
        0x50,
        0x27,
        0x00,
        0x02,
        0x01,
        0x01,
        0x03,
        0x21,
        0x09,
        0x07,
        0x00,
        0x02,
        0x00,
        0x00,
        0x0a,
        0x00,
        0x00,
        0x07,
        0x05,
        0x81,
        0x03,
        0x08,
        0x00,
        0x0a,
        // A control endpoint, its interval is zero.
        0x07,
        0x05,
        0x80,
        0x02,
        0x08,
        0x00,
        0x00,
    };

    const auto descriptors = Kbi::parseUsbDescriptors(raw);
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
    const auto& controlInterval = descriptors[3].fields[5];
    check(controlInterval.children.size() == 3, "the control endpoint is decoded");
    if (controlInterval.children.size() == 3)
        check(controlInterval.children[2].value == "InfinityHz",
            "an interval of zero does not divide by zero");
}

void testPointSnapper()
{
    std::println("point snapper");

    Kbi::Ui::PointSnapper snapper;
    check(snapper.find(0.0, 0.0, 40.0) == nullptr, "an empty snapper finds nothing");

    // The points are not sorted, the snapper sorts them.
    snapper.set({ { .x = 30.0, .y = 5.0, .index = 2 }, { .x = 10.0, .y = 5.0, .index = 0 }, { .x = 20.0, .y = 5.0, .index = 1 } });

    const Kbi::Ui::SnapPoint* snapped = snapper.find(11.0, 5.0, 40.0);
    check(snapped != nullptr, "a point is found");
    if (snapped != nullptr) {
        check(snapped->index == 0, "the closest point is found");
        check(snapped->x == 10.0, "the position of the point is kept");
    }

    // A point that is too far away is not found.
    check(snapper.find(500.0, 500.0, 40.0) == nullptr, "a point outside of the radius is not found");

    // The closest point wins, not the first one.
    snapper.set({ { .x = 10.0, .y = 100.0, .index = 0 }, { .x = 12.0, .y = 0.0, .index = 1 } });
    snapped = snapper.find(11.0, 0.0, 40.0);
    check(snapped != nullptr && snapped->index == 1, "the closest point wins");

    // The radius is measured in pixels.
    snapper.set({ { .x = 10.0, .y = 0.0, .index = 0 } });
    check(snapper.find(10.0, 39.0, 40.0) != nullptr, "a point inside the radius is found");
    check(snapper.find(10.0, 41.0, 40.0) == nullptr, "a point outside the radius is not found");
}

void testParser(const std::string& path)
{
    std::println("parser");
    const auto result = Kbi::parseResultFile(path);
    check(result.has_value(), "the recording is parsed");
    if (!result)
        return;
    check(result->inputs.size() == 2, "both devices are present");
    check(result->devices.size() == 2, "the device list is parsed");
    check(result->usbDevices.size() == 1, "the usb device list is parsed");
    check(result->totalInputCount() == 1240, "all input events are parsed");
    check(result->info.backend == "evdev", "the metadata is parsed");
}

void testDatetime()
{
    std::println("datetime");

    // The .NET epoch is 0001-01-01, the unix epoch is 1970-01-01.
    const auto unixEpoch = std::chrono::system_clock::time_point {};
    check(Kbi::formatIso8601Utc(unixEpoch) == "1970-01-01T00:00:00Z",
        "the unix epoch is formatted correctly");

    // 2026-09-26T18:39:27.211Z, the time of the "SayoDevice 7K mechanical.kbi"
    // recording.
    const std::uint64_t ticks = 639'260'447'672'114'184;
    const auto timePoint = Kbi::fromNetDatetimeTicks(ticks);
    check(Kbi::formatIso8601Utc(timePoint, true) == "2026-09-26T18:39:27.211Z",
        "the .NET ticks are converted correctly (" + Kbi::formatIso8601Utc(timePoint, true) + ")");

    // The two upper bits hold the DateTimeKind, they are not part of the time.
    const std::uint64_t withKind = ticks | (2ULL << 62);
    check(Kbi::fromNetDatetimeTicks(withKind) == timePoint,
        "the DateTimeKind bits are ignored");
    check(Kbi::fromNetDatetimeTicks(ticks | (1ULL << 62)) == timePoint,
        "the Utc kind is ignored");

    // The round trip through the .NET DateTime encoding, the same conversion
    // the .kbi exporter uses.
    const auto roundTrip = Kbi::fromNetDatetimeTicks(
        Kbi::toNetDatetimeTicks(timePoint, Kbi::kNetDateTimeKindUtc));
    check(roundTrip == timePoint, "the .NET DateTime encoding round trips");

    // The kind bits are set, but they do not change the time.
    check(Kbi::toNetDatetimeTicks(timePoint, Kbi::kNetDateTimeKindUtc) != Kbi::toNetDatetimeTicks(timePoint, Kbi::kNetDateTimeKindLocal),
        "the DateTimeKind is stored in the upper bits");
    check((Kbi::toNetDatetimeTicks(timePoint, Kbi::kNetDateTimeKindUtc) >> 62) == 1,
        "the Utc kind is written");

    // A time before the unix epoch.
    const auto before = Kbi::fromNetDatetimeTicks(621'355'968'000'000'000ULL - 1);
    check(Kbi::formatIso8601Utc(before) == "1969-12-31T23:59:59Z",
        "times before the unix epoch are converted correctly (" + Kbi::formatIso8601Utc(before) + ")");
}

void testLegacyParser(const std::string& path)
{
    std::println("legacy kbi parser");
    const auto result = Kbi::parseResultFile(path);
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
    check(result->totalInputCount() == 8, "all events are parsed");
    check(result->time == "2025-07-22T22:34:43.000Z",
        "the recording time is converted correctly (" + result->time + ")");

    const auto& keyboard = result->inputs.at("HID_VID_3149&PID_5041&MI_00");
    check(keyboard.size() == 4, "the keyboard events are grouped by source");
    check(keyboard[0].timestamp == 0, "the first timestamp is parsed");
    check(keyboard[0].code == 0x04, "the key name is mapped to a keycode");
    check(!keyboard[1].pressed, "the release event is parsed");
    check(keyboard[2].code == 0x1E, "the digit key name is mapped to a keycode");
    check(result->inputs.at("HID_VID_3149&PID_5042&MI_00")[1].code == 0x3E,
        "the function key name is mapped to a keycode");
}

} // namespace

namespace {

// Dumps the analysis of a recording so it can be compared against a reference
// implementation of the web analyzer.
int dumpAnalysis(const std::string& path, const std::string& outputPath)
{
    const auto result = Kbi::parseResultFile(path);
    if (!result) {
        std::println(stderr, "Could not parse {}", path);
        return 1;
    }

    std::vector<double> timestamps;
    for (const auto& [id, events] : result->inputs)
        for (const Kbi::InputEvent& event : events)
            timestamps.push_back(event.timestamp);
    std::sort(timestamps.begin(), timestamps.end());

    Kbi::Analyzer analyzer;
    analyzer.add(timestamps);

    // Wait for the worker thread to produce the frequency domain snapshot.
    std::shared_ptr<const Kbi::FreqSnapshot> snapshot;
    for (int i = 0; i < 1000 && !snapshot; ++i) {
        snapshot = analyzer.freqSnapshot();
        if (!snapshot)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!snapshot) {
        std::println(stderr, "The frequency snapshot was not ready");
        return 1;
    }

    std::ofstream out(outputPath);
    out.precision(17);
    if (!out) {
        std::println(stderr, "Could not write {}", outputPath);
        return 1;
    }

    const auto dump = [&out](const char* name, const std::vector<double>& values) {
        out << name << " " << values.size();
        for (const double value : values)
            out << " " << value;
        out << "\n";
    };

    dump("consecutive_diff", analyzer.consecutiveDiff());
    dump("all_diff", analyzer.allDiff());
    dump("wrapped_timestamp", analyzer.wrappedTimestamp());
    dump("consecutive_diff_freq", snapshot->consecutiveDiff);
    dump("all_diff_freq", snapshot->allDiff);
    dump("wrapped_timestamp_freq", snapshot->wrappedTimestamp);
    return 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc > 3 && std::string(argv[1]) == "--dump")
        return dumpAnalysis(argv[2], argv[3]);

    testFft();
    testAnalyzer();
    testDecimate();
    testPostprocess();
    testTimeline();
    testBase64();
    testDatetime();
    testUsbPollingRate();
    testChartInteraction();
    testPointSnapper();
    if (argc > 1)
        testParser(argv[1]);
    if (argc > 2)
        testLegacyParser(argv[2]);

    if (gFailures == 0) {
        std::println("\nAll tests passed.");
        return 0;
    }
    std::println("\n{} test(s) failed.", gFailures);
    return 1;
}
