# Keyboard Inspector — native analyzer

A native port of the web analyzer (`../analyzer`) built with Dear ImGui and
ImPlot. The recorder (`../recorder`) is linked in as a library, so no external
process is spawned any more.

The analyzer also builds to WebAssembly with Emscripten, so it can run in a
browser. Recording is only available in the native build; the web build
analyses recordings that were captured elsewhere.

## Features

* Opens the recorder's JSON results and the legacy binary `.kbi` recordings
  with the native file dialog (TinyFileDialogs). The web build uses a browser
  file picker instead.
* Records live through the embedded `recorder-lib` (evdev on Linux, GameInput
  or RawInput on Windows). This is only available in the native build.
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
  project of the repository are declared in the single manifest at the root of
  the repository, see [`../vcpkg.json`](../vcpkg.json). Dependencies that only
  the native build needs are excluded from the WebAssembly build with vcpkg
  platform expressions.
* [Emscripten](https://emscripten.org/) (and its `EMSDK` environment) for the
  WebAssembly build.

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

### WebAssembly

The analyzer also builds for the browser with
[Emscripten](https://emscripten.org/):

```sh
source ~/emsdk/emsdk_env.sh
cmake --preset emscripten
cmake --build --preset emscripten
```

The build writes `build-wasm/analyzer-native/analyzer-native.html`, `.js` and
`.wasm`. The page has to be served over HTTP, opening the HTML file directly
does not work. `emrun` starts a server and a browser for it:

```sh
emrun --browser chrome build-wasm/analyzer-native/analyzer-native.html
```

The FFT runs on a pthread in the browser as well, which needs
`SharedArrayBuffer`, so the page has to be cross-origin isolated. `emrun`
already sends the `Cross-Origin-Opener-Policy: same-origin` and
`Cross-Origin-Embedder-Policy: require-corp` headers; other static file servers
have to be configured to send them too, otherwise the analyzer fails to start
with a "SharedArrayBuffer is not available" error. The Cloudflare Pages
deployment sends them through the `web/_headers` file (see Deployment below).

Recording is not available in the browser, so the web build only has the
"From file" mode. The open file dialog is implemented by a JS shim
(`web/file_dialog.js`): it shows a hidden `<input type="file">`, copies the
selected file into Emscripten's in-memory filesystem and hands the path to the
regular C++ parser. The shim suspends the WebAssembly stack with Asyncify until
the browser closed the picker, so the rest of the application can keep using
the synchronous `openRecordingDialog()` interface.

The web build links only the parts of the recorder that it needs (the keycode
names and the `.kbi` date conversion); the platform dependent recorder sources
are replaced by the file parser. It uses the same GLFW + OpenGL 3 backend as
the native build, with Emscripten's GLFW implementation and a WebGL 2 context.
The FFT is computed on the same worker thread as in the native build. Because
`-pthread` switches the wasm module to shared memory, the dependencies are
built with `-pthread` too. The overlay triplet at
`web/triplets/wasm32-emscripten.cmake` adds that flag to vcpkg's community
Emscripten triplet, along with `IMGUI_IMPL_OPENGL_ES3` so that the prebuilt
ImGui library uses its GLES3 path.

### Deployment

The CI publishes the web build to
[Cloudflare Pages](https://pages.cloudflare.com/): a push to `main` becomes a
production deployment at `https://keyboard-inspector.pages.dev`, and a manual
run of the workflow deploys the branch it was started from as a preview
deployment. The `analyzer-web-deploy` job needs two repository secrets:

* `CLOUDFLARE_API_TOKEN` — an API token with the
  [Cloudflare Pages — Edit](https://developers.cloudflare.com/cloudflare-one/authorizations/applications/permissions/)
  permission for the account.
* `CLOUDFLARE_ACCOUNT_ID` — the account ID from the Cloudflare dashboard
  overview page.

The Pages project is created on the first deployment, or before that with
`npx wrangler pages project create keyboard-inspector`.

The build output is packaged for static hosting: the entry point is copied to
the conventional `index.html`, and `web/_headers` is placed next to it so the
hosting serves the two cross-origin isolation headers. A local build can be
deployed by hand with:

```sh
npx wrangler pages deploy build-wasm/analyzer-native --project-name=keyboard-inspector
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
| `--record` | start recording on startup (native only) |
| `--info` | open the info window on startup |
| `--screenshot <file.ppm>` | write a screenshot after 30 frames and exit |

The command line options are not available in the web build.

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
web/
  _headers                 cross-origin isolation headers for Cloudflare Pages
  file_dialog.js           JS shim for the browser file picker
  shell.html               the page the Emscripten build is embedded in
  triplets/                overlay triplet that builds the dependencies with -pthread
```

## Notes on the port

* The histograms are computed by the same algorithm as the web app. Each
  histogram entry covers `1e6 / bin_rate` microseconds, so the histograms are
  sampled at the bin rate and have one entry per hertz of the spectrum. The
  FFT is done by [PFFFT](https://bitbucket.org/jpommier/pffft/), which
  supports transform sizes of the form `2^a * 3^b * 5^c`: the length of the
  histograms is the bin rate (`125 * 2^n`), which PFFFT accepts directly, so
  no zero padding is needed and the frequency of a bin is its index in
  hertz. The spectrum covers every frequency from 0 to `bin_rate / 2` Hz,
  which is `bin_rate / 2 + 1` entries. The two small bin rates (1000 and
  2000 Hz) are rounded up to the next power of two, because PFFFT needs a
  size that is a multiple of 32. The transform runs on a worker thread so
  that the UI never blocks; in the browser the worker is an Emscripten
  pthread.
* The difference charts use milliseconds for the x axis. The web app labels
  the bin index as "ms", which only matches the bin index at a bin rate of
  1000 Hz.
* `recorder-lib` dereferences the logger it is given, so the app always hands
  it `spdlog::default_logger()`.
* The charts implement their own pan, zoom and reset instead of using the
  ImPlot input handling, so that all of them behave the same way and so that
  the charts of a row can share their zoom state. They clamp the zoom like the
  web app's d3 `scaleExtent` (diff charts 1000x, frequency charts 32x, the
  timeline 64x) and scale by `e^-0.2` per wheel step, like the web app.
* The legacy `.kbi` format stores fractional seconds, so the timestamps are
  kept as floating point microseconds, like the web app does.
* The histograms cover the timestamp differences below one second, the ones of
  exactly one second fall outside of them.
