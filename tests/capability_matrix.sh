#!/usr/bin/env bash
#
# The capability matrix (#5.6).
#
# capability x {on, off}, NOT the power set: 21 gateable capabilities is 2.1
# million combinations, which is not a plan. Cross-capability coverage comes from
# a named short list of PAIRS -- the implication edges, and MT_COMMERCIAL_BUILD
# against each capability whose commercial-mode effect is not `none`.
#
# The DummyApp is the only host that can afford a matrix at all; that is its job
# in this programme.
#
# Usage:
#   tests/capability_matrix.sh                 # every capability, off then on
#   tests/capability_matrix.sh MT_CAP_LLM ...  # only these
#   tests/capability_matrix.sh --pairs         # only the named pairs
#   tests/capability_matrix.sh --no-tests      # build only, skip the suites
#
# Each variant builds through the ordinary path -- ./build-macos.sh with an
# edited manifest -- rather than through a private code path, so a green matrix
# says something about what a user would get.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MANIFEST="$PROJECT_DIR/mtengine.caps"
BACKUP="$(mktemp -t mtengine.caps.XXXXXX)"
RESULTS="$PROJECT_DIR/tests/results/capability_matrix.txt"

RUN_TESTS=true
ONLY_PAIRS=false
SELECTED=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --no-tests) RUN_TESTS=false ;;
        --pairs)    ONLY_PAIRS=true ;;
        -h|--help)  sed -n '2,24p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)          SELECTED+=("$1") ;;
    esac
    shift
done

# The implication edges, plus the licence mode against the capabilities whose
# commercial-mode effect is `variant`. These are CHOSEN, not swept.
PAIRS=(
    "MT_CAP_NET_GAME=0 MT_CAP_NET_TRANSPORT=0 MT_CAP_HTTP=0 MT_CAP_HTTPS=0 MT_CAP_LLM=0"
    "MT_CAP_HTTPS=0 MT_CAP_LLM=0"
    "MT_CAP_COLOR_MANAGEMENT=0 MT_CAP_DEVELOP=0 MT_CAP_RAW=0"
    "MT_COMMERCIAL_BUILD=1"
    "MT_COMMERCIAL_BUILD=1 MT_CAP_VIDEO_PLAYBACK=0"
)

cleanup() {
    if [[ -f "$BACKUP" ]]; then
        cp "$BACKUP" "$MANIFEST"
        rm -f "$BACKUP"
        echo ""
        echo "(manifest restored)"
    fi
}
trap cleanup EXIT INT TERM

cp "$MANIFEST" "$BACKUP"
mkdir -p "$(dirname "$RESULTS")"
: > "$RESULTS"

PASS=0
FAIL=0

# ---------------------------------------------------------------------------
# run_variant <label> <KEY=VALUE> [KEY=VALUE ...]
#
# Applies the settings to the manifest, builds, optionally runs both suites, and
# records the result. The manifest is restored between variants, so a variant
# never inherits the previous one's edits.
# ---------------------------------------------------------------------------
run_variant() {
    local label="$1"; shift
    cp "$BACKUP" "$MANIFEST"

    local kv key value
    for kv in "$@"; do
        key="${kv%%=*}"
        value="${kv##*=}"
        # perl, not `sed -i ''`: the BSD form is macOS-only and this has to run
        # on three platforms.
        if grep -q "^${key}=" "$MANIFEST"; then
            perl -pi -e "s/^\Q${key}\E=.*/${key}=${value}/" "$MANIFEST"
        else
            printf '%s=%s\n' "$key" "$value" >> "$MANIFEST"
        fi
    done

    printf '%-56s ' "$label"

    local log; log="$(mktemp -t mtcapsmatrix.XXXXXX)"
    if ! ( cd "$PROJECT_DIR" && ./build-macos.sh --debug ) > "$log" 2>&1; then
        echo "BUILD FAILED"
        {
            echo "=== $label: BUILD FAILED ==="
            grep -E "error:|FATAL_ERROR|Undefined symbol|^mtcaps:|conflict:" "$log" | head -20
            echo ""
        } >> "$RESULTS"
        FAIL=$((FAIL + 1))
        rm -f "$log"
        return 1
    fi

    if [[ "$RUN_TESTS" == "true" ]]; then
        if ! ( cd "$PROJECT_DIR" && ./tests/run_test.sh --skip-build ) > "$log" 2>&1; then
            echo "TESTS FAILED"
            {
                echo "=== $label: TESTS FAILED ==="
                tail -30 "$log"
                echo ""
            } >> "$RESULTS"
            FAIL=$((FAIL + 1))
            rm -f "$log"
            return 1
        fi
    fi

    echo "ok"
    echo "=== $label: PASS ===" >> "$RESULTS"
    PASS=$((PASS + 1))
    rm -f "$log"
    return 0
}

# ---------------------------------------------------------------------------

if [[ "$ONLY_PAIRS" == "true" ]]; then
    CAPS=()
else
    if [[ ${#SELECTED[@]} -gt 0 ]]; then
        CAPS=("${SELECTED[@]+"${SELECTED[@]}"}")
    else
        # Read the capability list from the VOCABULARY, never from a list typed
        # here: a hand-maintained list is wrong within two capabilities.
        mapfile_compat() { while IFS= read -r l; do CAPS+=("$l"); done; }
        CAPS=()
        mapfile_compat < <(python3 -c "
import json, sys
v = json.load(open('$PROJECT_DIR/../MTEngineSDL/tools/mtcaps/vocabulary.json'))
for k in sorted(v['capabilities']):
    print(k)
")
    fi
fi

echo "=== Capability matrix: ${#CAPS[@]} capabilities, ${#PAIRS[@]} pairs ==="
echo ""

# TURNING OFF A CAPABILITY MEANS TURNING OFF EVERYTHING THAT IMPLIES IT.
#
# Measured, not anticipated: the first run of this matrix reported
# MT_CAP_HTTPS=0 as a BUILD FAILURE. It was not one. Every engine default is 1,
# so MT_CAP_LLM was still on, and LLM implies HTTPS -- a configure-time conflict,
# which is the generator doing exactly what it should. An explicit =0 never
# silently overrides an implication; only the manifest's author can say which
# half was meant.
#
# So a single-axis variant is "this capability and its whole implying closure
# off". Anything else tests the conflict detector rather than the capability.
closure_off() {
    python3 -c "
import json, sys
v = json.load(open('$PROJECT_DIR/../MTEngineSDL/tools/mtcaps/vocabulary.json'))
caps = v['capabilities']
target = sys.argv[1]
# reverse-implication closure: everything that transitively implies the target
off, frontier = {target}, [target]
while frontier:
    cur = frontier.pop()
    for k, c in caps.items():
        if cur in c['implies'] and k not in off:
            off.add(k); frontier.append(k)
print(' '.join('%s=0' % k for k in sorted(off)))
" "$1"
}

# "${CAPS[@]+...}", not a bare "${CAPS[@]}": bash 3.2 -- which is /bin/bash on
# macOS -- treats an EMPTY array as UNSET under `set -u`, and --pairs sets CAPS
# to exactly that. Verified: the bare form dies with "CAPS[@]: unbound variable"
# on line one of the loop.
#
# It survived until now only because this machine has bash 5 earlier on PATH than
# /bin/bash, and the shebang is `env bash`. That is the whole hazard: the trap is
# invisible on the machine that writes the script and fires on the machine that
# runs it.
for cap in "${CAPS[@]+"${CAPS[@]}"}"; do
    settings="$(closure_off "$cap")"
    extra=""
    if [[ "$settings" != "$cap=0" ]]; then
        extra=" (+$(( $(echo "$settings" | wc -w) - 1 )) implying)"
    fi
    # shellcheck disable=SC2086
    run_variant "$cap=0$extra" $settings
done

if [[ ${#CAPS[@]} -gt 0 ]]; then
    run_variant "baseline (all defaults)"
fi

for pair in "${PAIRS[@]+"${PAIRS[@]}"}"; do
    # shellcheck disable=SC2086
    run_variant "pair: $pair" $pair
done

echo ""
echo "=== $PASS passed, $FAIL failed ==="
echo "Details: $RESULTS"
[[ "$FAIL" -eq 0 ]]
