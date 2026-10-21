# Keyboard Inspector

Tools for recording and analysing keyboard input timing.

| Directory | Description |
| --- | --- |
| [`recorder/`](recorder) | The recorder library. Captures HID input events on Linux (evdev) and Windows (GameInput, RawInput). It is used as a library by the analyzer and as a standalone tool by the web app. |
| [`analyzer/`](analyzer) | The web app (Svelte/TypeScript, packaged with Neutralino). |
| [`analyzer-native/`](analyzer-native) | The native analyzer (C++23, Dear ImGui and ImPlot). |

## Building

The top level project builds the recorder and the native analyzer together.
Every dependency is declared in the single [`vcpkg.json`](vcpkg.json) at the
root of the repository.

```sh
export VCPKG_ROOT=~/vcpkg

cmake --preset "vcpkg - Ninja - Release"
cmake --build --preset "Ninja - Release"
```

The binaries are placed in `build/recorder/recorder-app` and
`build/analyzer-native/analyzer-native`.

Both projects can also be built on their own, they then use the same root
manifest:

```sh
cd recorder && cmake --preset "vcpkg - Ninja - Release" && cmake --build --preset "CI - Release"
cd analyzer-native && cmake --preset "vcpkg - Ninja - Release" && cmake --build --preset "Ninja - Release"
```

## Tests

```sh
ctest --test-dir build --output-on-failure
```
