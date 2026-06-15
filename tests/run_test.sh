#!/bin/bash
#
# CLI Test Runner for MTEngineSDLDummyApp
#
# Usage:
#   tests/run_test.sh [OPTIONS] [TestName]
#
# Options:
#   --skip-build    Skip the xcodebuild step
#   --timeout N     Timeout in seconds (default: 120)
#   --log-dir DIR   Log output directory (default: /tmp)
#
# Examples:
#   tests/run_test.sh                        # Run all CTestSuite tests
#   tests/run_test.sh AppStartup             # Run single test by name
#   tests/run_test.sh --skip-build AppStartup

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

SKIP_BUILD=false
TIMEOUT=120
TEST_NAME=""
LOG_DIR="/tmp"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-build) SKIP_BUILD=true; shift ;;
        --timeout)    TIMEOUT="$2"; shift 2 ;;
        --log-dir)    LOG_DIR="$2"; shift 2 ;;
        *)            TEST_NAME="$1"; shift ;;
    esac
done

RESULTS_DIR="$PROJECT_DIR/tests/results"
RESULTS_FILE="$RESULTS_DIR/last_run.txt"
XCODE_PROJECT="$PROJECT_DIR/platform/MacOS/MTEngineSDLDummyApp.xcodeproj"

mkdir -p "$RESULTS_DIR"

if [ "$SKIP_BUILD" = false ]; then
    echo "=== Building MTEngineSDLDummyApp ==="
    if ! xcodebuild -project "$XCODE_PROJECT" -scheme MTEngineSDLDummyApp -quiet 2>&1; then
        echo "BUILD FAILED"
        exit 2
    fi
    echo "=== Build succeeded ==="
fi

rm -f "$RESULTS_FILE"

# Find built binary in Xcode's default DerivedData (shared with Xcode.app)
APP_BINARY=""
for search_path in \
    "$PROJECT_DIR/platform/MacOS/DerivedData" \
    "$HOME/Library/Developer/Xcode/DerivedData"; do
    APP_BINARY=$(find "$search_path" \
        -name "MTEngineSDLDummyApp" -type f -perm -111 2>/dev/null \
        | grep -v "\.dSYM" | head -1 || true)
    [ -n "$APP_BINARY" ] && break
done

if [ -z "$APP_BINARY" ]; then
    echo "ERROR: Could not find MTEngineSDLDummyApp binary. Build first."
    exit 2
fi

echo "=== Using binary: $APP_BINARY ==="
cd "$PROJECT_DIR"

if [ -n "$TEST_NAME" ]; then
    echo "=== Running test: $TEST_NAME ==="
    "$APP_BINARY" --headless --log-dir "$LOG_DIR" --run-test "$TEST_NAME" --exit-after-tests &
else
    echo "=== Running all suite tests ==="
    "$APP_BINARY" --headless --log-dir "$LOG_DIR" --run-suite --exit-after-tests &
fi

APP_PID=$!
ELAPSED=0
while kill -0 "$APP_PID" 2>/dev/null; do
    sleep 1
    ELAPSED=$((ELAPSED + 1))
    if [ "$ELAPSED" -ge "$TIMEOUT" ]; then
        echo "TIMEOUT: Test did not complete within ${TIMEOUT}s"
        kill -TERM "$APP_PID" 2>/dev/null || true
        sleep 3
        kill -0 "$APP_PID" 2>/dev/null && kill -KILL "$APP_PID" 2>/dev/null || true
        wait "$APP_PID" 2>/dev/null || true
        exit 3
    fi
done
wait "$APP_PID" 2>/dev/null || true

if [ ! -f "$RESULTS_FILE" ]; then
    echo "ERROR: Results file not found at $RESULTS_FILE"
    echo "The application may have crashed before writing results."
    exit 1
fi

echo ""
echo "=== Test Results ==="
cat "$RESULTS_FILE"
echo ""

RESULT_LINE=$(grep "^RESULT:" "$RESULTS_FILE" || true)
if [ -z "$RESULT_LINE" ]; then
    echo "ERROR: No RESULT line found in results file"
    exit 1
fi

PASSED=$(echo "$RESULT_LINE" | sed 's/RESULT: \([0-9]*\)\/.*/\1/')
TOTAL=$(echo "$RESULT_LINE" | sed 's/RESULT: [0-9]*\/\([0-9]*\).*/\1/')

if [ "$PASSED" = "$TOTAL" ] && [ "$TOTAL" != "0" ]; then
    echo "ALL TESTS PASSED ($PASSED/$TOTAL)"
    exit 0
else
    echo "TESTS FAILED ($PASSED/$TOTAL passed)"
    exit 1
fi
