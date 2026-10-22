#!/usr/bin/env bash
#
# Runs clang-format over the C and C++ sources of this repository.
#
# The style is defined by the .clang-format file in the repository root and
# follows the WebKit code style guidelines:
# https://webkit.org/code-style-guidelines/
#
# Usage:
#   Tools/format.sh            # format every source file in place
#   Tools/format.sh --check    # fail if any source file is not formatted
#   Tools/format.sh --diff     # print a diff of the pending changes
#   Tools/format.sh --check FILE...  # only look at the given files
#
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

mode="fix"
explicit_files=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --check)
            mode="check"
            shift
            ;;
        --diff)
            mode="diff"
            shift
            ;;
        --fix)
            mode="fix"
            shift
            ;;
        -h | --help)
            sed -n '3,13p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            explicit_files+=("$1")
            shift
            ;;
    esac
done

clang_format="${CLANG_FORMAT:-clang-format}"
if ! command -v "$clang_format" >/dev/null 2>&1; then
    echo "error: $clang_format not found in PATH" >&2
    exit 127
fi

if [[ ${#explicit_files[@]} -gt 0 ]]; then
    files=("${explicit_files[@]}")
else
    mapfile -t files < <(
        git ls-files -z --cached --others --exclude-standard |
            tr '\0' '\n' |
            grep -E '\.(c|cc|cpp|cxx|h|hh|hpp|hxx|m|mm)$' |
            grep -v -E '^(analyzer|vcpkg_installed)/'
    )
fi

if [[ ${#files[@]} -eq 0 ]]; then
    echo "no source files to format"
    exit 0
fi

case "$mode" in
    fix)
        "$clang_format" -i --style=file "${files[@]}"
        echo "formatted ${#files[@]} files"
        ;;
    check)
        # --dry-run lists the files that would change and exits non-zero.
        output="$("$clang_format" --dry-run --Werror --style=file "${files[@]}" 2>&1)" && status=0 || status=$?
        if [[ $status -ne 0 ]]; then
            echo "$output"
            echo
            echo "error: the files listed above are not formatted" >&2
            echo "run 'Tools/format.sh' to fix them" >&2
            exit 1
        fi
        echo "clang-format: ${#files[@]} files are correctly formatted"
        ;;
    diff)
        for file in "${files[@]}"; do
            "$clang_format" --style=file "$file" |
                diff -u --label "a/$file" --label "b/$file" "$file" - || true
        done
        ;;
esac
