#!/bin/bash
#
# CLI Test Runner for MTEngineSDLDummyApp
#
# Runs BOTH suites by default -- the CTestSuite integration tests (--run-suite)
# and the imgui_test_engine UI tests (--run-tests) -- and fails if either does.
# Until 2026-08-23 it ran only the first, so the imgui suite executed only when
# somebody drove the binary by hand, which is how a suite goes quiet rather than
# red. Both write tests/results/last_run.txt in the same format.
#
# Usage:
#   tests/run_test.sh [OPTIONS] [TestName]
#
# Options:
#   --suite         Run only the CTestSuite integration tests
#   --imgui         Run only the imgui_test_engine UI tests
#   --skip-build    Skip the build step
#   --binary PATH   Use this binary instead of building/searching for one.
#                   This is what makes the runner usable on Linux and Windows:
#                   each platform's own build script produces the binary and
#                   hands it here, rather than this script learning three build
#                   systems. Implies --skip-build.
#   --timeout N     Timeout in seconds per suite (default: 120)
#   --log-dir DIR   Log output directory (default: /tmp)
#
# Examples:
#   tests/run_test.sh                        # both suites
#   tests/run_test.sh --suite AppStartup     # one CTestSuite test by name
#   tests/run_test.sh --skip-build
#   tests/run_test.sh --binary ./build/MTEngineSDLDummyApp

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

SKIP_BUILD=false
TIMEOUT=120
TEST_NAME=""
LOG_DIR="/tmp"
APP_BINARY=""
RUN_SUITE=true
RUN_IMGUI=true

while [[ $# -gt 0 ]]; do
    case "$1" in
        --suite)      RUN_SUITE=true;  RUN_IMGUI=false; shift ;;
        --imgui)      RUN_SUITE=false; RUN_IMGUI=true;  shift ;;
        --skip-build) SKIP_BUILD=true; shift ;;
        --binary)     APP_BINARY="$2"; SKIP_BUILD=true; shift 2 ;;
        --timeout)    TIMEOUT="$2"; shift 2 ;;
        --log-dir)    LOG_DIR="$2"; shift 2 ;;
        -h|--help)    sed -n '2,29p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)            TEST_NAME="$1"; shift ;;
    esac
done

# A named test is a CTestSuite concept; there is no per-test selector on the
# imgui path. Asking for one and silently running the whole other suite as well
# would report a pass nobody asked for.
if [ -n "$TEST_NAME" ]; then
    RUN_IMGUI=false
    RUN_SUITE=true
fi

RESULTS_DIR="$PROJECT_DIR/tests/results"
RESULTS_FILE="$RESULTS_DIR/last_run.txt"
XCODE_PROJECT="$PROJECT_DIR/platform/MacOS/MTEngineSDLDummyApp.xcodeproj"

# Build and run a single, explicit configuration so we never launch a stale
# binary from the other configuration. `xcodebuild -scheme ...` defaults to
# Debug for the build action, so match that here.
CONFIGURATION="Debug"

mkdir -p "$RESULTS_DIR"

if [ "$SKIP_BUILD" = false ]; then
    if [ "$(uname -s)" != "Darwin" ]; then
        echo "ERROR: this runner only knows how to BUILD on macOS."
        echo "       On Linux/Windows, build with that platform's script and pass"
        echo "       the result:  tests/run_test.sh --binary <path>"
        exit 2
    fi
    echo "=== Building MTEngineSDLDummyApp ($CONFIGURATION) ==="
    if ! xcodebuild -project "$XCODE_PROJECT" -scheme MTEngineSDLDummyApp -configuration "$CONFIGURATION" -quiet 2>&1; then
        echo "BUILD FAILED"
        exit 2
    fi
    echo "=== Build succeeded ==="
fi

if [ -z "$APP_BINARY" ]; then
    # WHICH CONFIGURATIONS TO SEARCH.
    #
    # When this script BUILT, it built exactly one configuration and must run
    # that one: picking the newest across both could launch a stale binary from
    # a build this script did not do.
    #
    # Under --skip-build there is no "configuration we just built", and pinning
    # Debug is actively wrong: ./build-macos.sh builds RELEASE, so
    # `./build-macos.sh && tests/run_test.sh --skip-build` silently tested a
    # stale Debug binary and reported a pass for code that was never in it.
    # (Found the hard way, 2026-08-24, while adding the HDR bench: the suite
    # reported 3/3 and 8/8 for a binary that contained neither new test.)
    # Search both and take the newest -- there is no configuration to be
    # faithful to, only a most-recent build.
    if [ "$SKIP_BUILD" = true ]; then
        SEARCH_CONFIGS="Debug Release"
    else
        SEARCH_CONFIGS="$CONFIGURATION"
    fi

    for search_path in \
        "$PROJECT_DIR/platform/MacOS/DerivedData" \
        "$HOME/Library/Developer/Xcode/DerivedData"; do
        CANDIDATES=""
        for cfg in $SEARCH_CONFIGS; do
            CANDIDATES="$CANDIDATES
$(find "$search_path" \
            -name "MTEngineSDLDummyApp" -type f -perm -111 \
            -path "*/Build/Products/$cfg/*" 2>/dev/null \
            | grep -v "\.dSYM" || true)"
        done
        CANDIDATES=$(echo "$CANDIDATES" | grep -v '^$' || true)
        if [ -n "$CANDIDATES" ]; then
            APP_BINARY=$(echo "$CANDIDATES" | while read -r f; do
                stat -f "%m %N" "$f"
            done | sort -rn | head -1 | cut -d' ' -f2-)
        fi
        [ -n "$APP_BINARY" ] && break
    done
fi

if [ -z "$APP_BINARY" ] || [ ! -x "$APP_BINARY" ]; then
    echo "ERROR: no MTEngineSDLDummyApp binary found. Build first, or pass --binary."
    exit 2
fi

echo "=== Using binary: $APP_BINARY ==="
cd "$PROJECT_DIR"

# ---------------------------------------------------------------------------
# run_one <label> <flag...>
#
# Runs one suite under the watchdog, then parses tests/results/last_run.txt.
# Each suite is run separately and its results file consumed before the next
# starts, because both write the same path.
# ---------------------------------------------------------------------------
run_one() {
    local label="$1"; shift

    rm -f "$RESULTS_FILE"
    echo ""
    echo "=== Running $label ==="

    "$APP_BINARY" --headless --log-dir "$LOG_DIR" "$@" --exit-after-tests &
    local pid=$!
    local elapsed=0
    while kill -0 "$pid" 2>/dev/null; do
        sleep 1
        elapsed=$((elapsed + 1))
        if [ "$elapsed" -ge "$TIMEOUT" ]; then
            echo "TIMEOUT: $label did not complete within ${TIMEOUT}s"
            kill -TERM "$pid" 2>/dev/null || true
            sleep 3
            kill -0 "$pid" 2>/dev/null && kill -KILL "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
            return 3
        fi
    done
    wait "$pid" 2>/dev/null || true

    if [ ! -f "$RESULTS_FILE" ]; then
        echo "ERROR: $label wrote no results file at $RESULTS_FILE"
        echo "       The application may have crashed before writing results."
        return 1
    fi

    echo "--- $label results ---"
    cat "$RESULTS_FILE"

    local result_line
    result_line=$(grep "^RESULT:" "$RESULTS_FILE" || true)
    if [ -z "$result_line" ]; then
        echo "ERROR: no RESULT line in $label results"
        return 1
    fi

    local passed total
    passed=$(echo "$result_line" | sed 's/RESULT: \([0-9]*\)\/.*/\1/')
    total=$(echo  "$result_line" | sed 's/RESULT: [0-9]*\/\([0-9]*\).*/\1/')

    # A zero-test run is a failure, not a pass. That is the shape a suite takes
    # when it silently stops being registered.
    if [ "$passed" = "$total" ] && [ "$total" != "0" ]; then
        echo "$label: PASSED ($passed/$total)"
        SUMMARY="${SUMMARY}${label}: ${passed}/${total} PASS"$'\n'
        return 0
    fi
    echo "$label: FAILED ($passed/$total passed)"
    SUMMARY="${SUMMARY}${label}: ${passed}/${total} FAIL"$'\n'
    return 1
}

SUMMARY=""
STATUS=0

if [ "$RUN_SUITE" = true ]; then
    if [ -n "$TEST_NAME" ]; then
        run_one "CTestSuite[$TEST_NAME]" --run-test "$TEST_NAME" || STATUS=$?
    else
        run_one "CTestSuite" --run-suite || STATUS=$?
    fi
fi

if [ "$RUN_IMGUI" = true ]; then
    run_one "ImGuiTests" --run-tests || STATUS=$?
fi

echo ""
echo "=== Summary ==="
printf '%s' "$SUMMARY"
if [ "$STATUS" -eq 0 ]; then
    echo "ALL SUITES PASSED"
else
    echo "FAILED (exit $STATUS)"
fi
exit "$STATUS"
