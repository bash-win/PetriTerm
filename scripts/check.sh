#!/usr/bin/env bash
#
# Everything CI will check, in the order CI checks it, so a failure shows up here
# rather than on a pushed branch. The file globs are copied from
# .github/workflows/ci.yml deliberately: if they drift apart this script stops
# being worth running, so change both together.
#
# CI also builds the whole matrix twice, once per compiler. This runs whatever
# compiler CMake picks up, so mirror the other arm with:
#
#   CXX=clang++ BUILD_DIRECTORY=build-clang scripts/check.sh
#
# Usage: scripts/check.sh [--skip-tidy]
#
#   --skip-tidy   Leave out static analysis, which is by far the slowest stage.
#                 For a quick loop while iterating; not enough before pushing.

set -euo pipefail

readonly repositoryRoot="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly buildDirectory="${BUILD_DIRECTORY:-build}"

skipStaticAnalysis=false
for argument in "$@"; do
    case "${argument}" in
        --skip-tidy) skipStaticAnalysis=true ;;
        *)
            echo "check.sh: unknown argument '${argument}'" >&2
            exit 2
            ;;
    esac
done

cd "${repositoryRoot}"

# Bold, and only when stdout is a terminal, so piping this into a file or a log
# does not fill it with escape sequences.
if [[ -t 1 ]]; then
    readonly headingStart=$'\e[1m'
    readonly headingEnd=$'\e[0m'
else
    readonly headingStart=""
    readonly headingEnd=""
fi

announceStage() {
    echo "${headingStart}==> $1${headingEnd}"
}

announceStage "Formatting (clang-format)"
# Kept as one word-split expansion rather than an array to match CI exactly. No
# path in this tree has spaces in it, and if one ever does, CI breaks first.
formatTargets=$(find src include tests -name '*.cpp' -o -name '*.hpp')
# shellcheck disable=SC2086
clang-format --dry-run --Werror ${formatTargets}

announceStage "Configure (${buildDirectory})"
# Regenerates compile_commands.json, which clang-tidy below reads to recover each
# file's real flags. Running configure unconditionally keeps the database honest
# after a source file is added or removed.
cmake -S . -B "${buildDirectory}" -G Ninja -DCMAKE_BUILD_TYPE=Debug

if [[ "${skipStaticAnalysis}" == false ]]; then
    announceStage "Static analysis (clang-tidy)"
    tidyTargets=$(find src -name '*.cpp')
    # shellcheck disable=SC2086
    clang-tidy -p "${buildDirectory}" ${tidyTargets}
fi

announceStage "Build"
# -Wall -Wextra -Wpedantic -Werror comes from CMakeLists.txt, so any warning
# introduced here fails this stage rather than being scrolled past.
cmake --build "${buildDirectory}" -j

announceStage "Tests"
ctest --test-dir "${buildDirectory}" --output-on-failure --no-tests=ignore

announceStage "All checks passed"
