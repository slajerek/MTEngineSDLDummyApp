#!/usr/bin/env bash
set -euo pipefail

# Build MTEngineSDLDummyApp for macOS via Xcode.
# Clones MTEngineSDL + uSockets if not present, stages uSockets.a, then builds via xcodebuild.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MTENGINE_DIR="$SCRIPT_DIR/../MTEngineSDL"
USOCKETS_DIR="$SCRIPT_DIR/../uSockets"

# Step 1: Clone MTEngineSDL if not present
if [[ ! -d "$MTENGINE_DIR" ]]; then
    echo "Cloning MTEngineSDL library repository"
    git clone --recursive https://github.com/slajerek/MTEngineSDL.git "$MTENGINE_DIR"
else
    ( cd "$MTENGINE_DIR" && git submodule update --init --recursive )
fi

# Step 2: Clone uSockets if not present
if [[ ! -d "$USOCKETS_DIR" ]]; then
    echo "Cloning uSockets library repository"
    git clone https://github.com/uNetworking/uSockets.git "$USOCKETS_DIR"
fi

# Step 3: Build uSockets and stage uSockets.a for MTEngineSDL (macOS)
echo "Building uSockets"
( cd "$USOCKETS_DIR" && make -j"$(sysctl -n hw.ncpu)" )
mkdir -p "$MTENGINE_DIR/platform/MacOS/libs/"
cp -f "$USOCKETS_DIR/uSockets.a" "$MTENGINE_DIR/platform/MacOS/libs/"

# Step 4: Build MTEngineSDLDummyApp via Xcode
echo "Building MTEngineSDLDummyApp (Release) via xcodebuild..."

XCODE_EXTRA=()
if command -v brew >/dev/null 2>&1; then
    XCODE_EXTRA+=("OTHER_LIBTOOLFLAGS=-L$(brew --prefix)/lib")
fi

# Use Xcode's default DerivedData so the build is shared with Xcode.app.
xcodebuild \
    -project platform/MacOS/MTEngineSDLDummyApp.xcodeproj \
    -scheme MTEngineSDLDummyApp \
    -configuration Release \
    CODE_SIGN_IDENTITY="-" \
    CODE_SIGNING_REQUIRED=NO \
    CODE_SIGNING_ALLOWED=NO \
    "${XCODE_EXTRA[@]}" \
    build

APP_BINARY=$(find "$HOME/Library/Developer/Xcode/DerivedData" \
    -name "MTEngineSDLDummyApp" -type f -perm -111 2>/dev/null \
    | grep -v "\.dSYM" | grep "MTEngineSDLDummyApp-" | head -1 || true)

if [[ -n "$APP_BINARY" ]]; then
    echo "MTEngineSDLDummyApp built: $APP_BINARY"
else
    echo "ERROR: build did not produce MTEngineSDLDummyApp binary" >&2
    exit 1
fi
