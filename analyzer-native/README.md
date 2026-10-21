# Keyboard Inspector — native analyzer

A native port of the web analyzer (`../analyzer`) built with Dear ImGui and
ImPlot. The recorder (`../recorder`) is linked in as a library, so no external
process is spawned any more.

## Features

* Opens the recorder's JSON results and the legacy binary `.kbi` recordings
  with the native file dialog (TinyFileDialogs).
* Records live through the embedded `recorder-lib` (evdev on Linux, GameInput
  or RawInput on Windows).
* The same seven charts as the web app, drawn with ImPlot:
  * three time domain histograms of the timestamp differences,
  * three FFT magnitude charts of those histograms,
  * a keypress timeline.
* The charts of a row share their zoom state, and support panning, wheel
  zooming and double click to reset.
* Every chart has a tooltip that describes the data point under the cursor,
  like the web app.
* An info window with the recording summary, the device list and the decoded
  USB descriptors.

## Requirements

* CMake 3.24 or newer
* A C++23 compiler
* [vcpkg](https://github.com/microsoft/vcpkg). The dependencies of every
  project of the repository are declared in the manifest at the root of the
  repository, see [`../vcpkg.json`](../vcpkg.json).

## Building

The analyzer is part of the top level project, which builds the recorder and
the analyzer together:

```sh
export VCPKG_ROOT=~/vcpkg

cd ..
cmake --preset "vcpkg - Ninja - Release"
cmake --build --preset "Ninja - Release"
```

The analyzer can also be built on its own, it then adds the recorder to the
project itself:

```sh
cmake --preset "vcpkg - Ninja - Release"
cmake --build --preset "Ninja - Release"
```

## Running

```sh
./build/analyzer-native/analyzer-native
```

Recording requires access to the input devices: on Linux you need to be root
or a member of the `input` group.

### Command line

| Option | Description |
| --- | --- |
| `--open <recording>` | open a `.json` or `.kbi` recording on startup |
| `--record` | start recording on startup |
| `--info` | open the info window on startup |
| `--screenshot <file.ppm>` | write a screenshot after 30 frames and exit |

## Tests

```sh
ctest --test-dir build --output-on-failure
```

`tests/reference_analyzer.py` is a reference implementation of the web
analyzer written in Python. It cross-checks the native histograms and FFT
against `numpy`:

```sh
./build/analyzer-native/analyzer-native-tests --dump tests/data/test_recording.json /tmp/dump.txt
python3 tests/reference_analyzer.py tests/data/test_recording.json /tmp/dump.txt
```

## Layout

```
src/
  main.cpp                 GLFW + OpenGL 3 backend, main loop
  app/                     application shell, file dialog, theme, screenshot
  core/                    analyzer, FFT, decimation, timeline, keycodes
  parser/                  JSON, legacy .kbi and USB descriptor parsers
  recorder/                the embedded recorder and the host system info
  ui/                      the ImPlot charts, the tooltips and the info window
```

## Notes on the port

* The histograms are computed by the same algorithm as the web app. The FFT is
  done by [PFFFT](https://bitbucket.org/jpommier/pffft/), which only supports
  transform sizes of the form `2^a * 3^b * 5^c`, so the histograms are zero
  padded to the next power of two. The spectrum therefore has a slightly finer
  resolution than the one of the web app, but the peaks stay at the same
  frequencies. The transform runs on a worker thread so that the UI never
  blocks.
* The difference charts use milliseconds for the x axis. The web app labels
  the bin index as "ms", which only matches the bin index at a bin rate of
  1000 Hz.
* `recorder-lib` dereferences the logger it is given, so the app always hands
  it `spdlog::default_logger()`.
* The charts implement their own pan, zoom and reset instead of using the
  ImPlot input handling, so that all of them behave the same way and so that
  the charts of a row can share their zoom state.
* The legacy `.kbi` format stores fractional seconds, so the timestamps are
  kept as floating point microseconds, like the web app does.
