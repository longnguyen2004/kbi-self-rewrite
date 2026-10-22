#!/usr/bin/env bash
#
# Runs clang-tidy over the C and C++ sources of this repository.
#
# The rules are defined by the .clang-tidy file in the repository root and
# follow the WebKit code style guidelines:
# https://webkit.org/code-style-guidelines/
#
# A compile database is required. Configure the project first:
#   cmake --preset "vcpkg - Ninja - Release"
#
# Usage:
#   Tools/tidy.sh                    # check every source file
#   Tools/tidy.sh --fix              # apply the suggested fixes
#   Tools/tidy.sh --warnings-as-errors  # fail on any warning (used by CI)
#   Tools/tidy.sh --build-dir DIR    # use a different build directory
#   Tools/tidy.sh FILE...            # only check the given files
#
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

build_dir="${BUILD_DIR:-build}"
fix=0
warnings_as_errors=0
checks=""
explicit_files=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --fix)
            fix=1
            shift
            ;;
        --warnings-as-errors)
            warnings_as_errors=1
            shift
            ;;
        --checks)
            checks="$2"
            shift 2
            ;;
        --checks=*)
            checks="${1#--checks=}"
            shift
            ;;
        --build-dir)
            build_dir="$2"
            shift 2
            ;;
        --build-dir=*)
            build_dir="${1#--build-dir=}"
            shift
            ;;
        -h | --help)
            sed -n '3,17p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            explicit_files+=("$1")
            shift
            ;;
    esac
done

clang_tidy="${CLANG_TIDY:-clang-tidy}"
if ! command -v "$clang_tidy" >/dev/null 2>&1; then
    echo "error: $clang_tidy not found in PATH" >&2
    exit 127
fi

if [[ ! -f "$build_dir/compile_commands.json" ]]; then
    echo "error: no compile_commands.json in '$build_dir'." >&2
    echo "configure the project first:" >&2
    echo "  cmake --preset \"vcpkg - Ninja - Release\"" >&2
    exit 1
fi

args=(-p "$build_dir")

if [[ -n "$checks" ]]; then
    args+=(--checks="$checks")
fi

if [[ $fix -eq 1 ]]; then
    args+=(--fix)
fi

if [[ $warnings_as_errors -eq 1 ]]; then
    args+=(--warnings-as-errors="*")
fi

if [[ ${#explicit_files[@]} -gt 0 ]]; then
    echo "== clang-tidy ${explicit_files[*]}"
    "$clang_tidy" "${args[@]}" "${explicit_files[@]}"
    exit $?
fi

run_clang_tidy="${RUN_CLANG_TIDY:-run-clang-tidy}"
if ! command -v "$run_clang_tidy" >/dev/null 2>&1; then
    echo "error: $run_clang_tidy not found in PATH" >&2
    echo "clang-tidy is installed as '$clang_tidy', but the driver script is missing" >&2
    exit 127
fi

# The driver builds a regex out of the positional arguments and matches it
# against the absolute paths in the compile database.
echo "== clang-tidy ($(grep -c '"file"' "$build_dir/compile_commands.json" 2>/dev/null || echo '?') translation units)"

driver_args=(-p "$build_dir" -j "$(nproc 2>/dev/null || echo 4)")
[[ -n "$checks" ]] && driver_args+=("-checks=$checks")
[[ $fix -eq 1 ]] && driver_args+=(-fix)
[[ $warnings_as_errors -eq 1 ]] && driver_args+=('-warnings-as-errors=*')
driver_args+=(recorder/src recorder/include analyzer-native/src analyzer-native/tests)

"$run_clang_tidy" "${driver_args[@]}"
