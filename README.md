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

## Code style

The C++ sources follow the [WebKit code style guidelines][webkit-style]. The
rules are enforced with `clang-format` and `clang-tidy`, configured in
[`.clang-format`](.clang-format) and [`.clang-tidy`](.clang-tidy) in the
repository root.

[webkit-style]: https://webkit.org/code-style-guidelines/

The two configurations are derived from WebKit's own, so they track the
upstream rules closely:

* `.clang-format` is WebKit's configuration with the options that only apply
  to Objective-C removed and the include categories adapted to this project.
* `.clang-tidy` enables the checks that match the guidelines and documents
  in a comment which rule every group of checks comes from. Checks that
  contradict the guidelines are switched off explicitly, for example
  `portability-avoid-pragma-once`, because WebKit requires `#pragma once`.

### Formatting

```sh
Tools/format.sh            # format every source file in place
Tools/format.sh --check    # fail if a source file is not formatted
Tools/format.sh --diff     # show what would change
```

The same is available as a CMake target:

```sh
cmake --build build --target format
cmake --build build --target format-check
```

### Static analysis

`clang-tidy` needs a compile database, so configure the project first.

```sh
Tools/tidy.sh                    # check every source file
Tools/tidy.sh --fix              # apply the suggested fixes
Tools/tidy.sh --warnings-as-errors  # fail on any warning, used by CI
Tools/tidy.sh --checks='-*,modernize-use-override'  # only run some checks
```

Or through CMake:

```sh
cmake --build build --target tidy
```

Note that `clang-tidy --fix` cannot rewrite every kind of use, so a few
findings have to be fixed by hand after a run. The rules that are known to
need this are listed in `.clang-tidy`:

* Namespace aliases keep their lower case name, `clang-tidy` cannot rewrite
  the uses of an aliased namespace.
* Type aliases are not renamed, the standard library contracts require
  specific names such as `value_type`.
* `boost::json` finds its `tag_invoke` customization point by name, so it
  keeps its snake_case spelling.

### Continuous integration

The [style workflow](.github/workflows/style.yml) runs the formatting check
and `clang-tidy` on every push and pull request. It pins the LLVM version so
that the output of `clang-format` matches the one used locally.

