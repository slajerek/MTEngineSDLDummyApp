#!/usr/bin/env bash
set -euo pipefail

# Build MTEngineSDLDummyApp for Linux.
# Clones MTEngineSDL + uSockets if not present, builds the engine, then builds DummyApp via CMake.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MTENGINE_DIR="$SCRIPT_DIR/../MTEngineSDL"
USOCKETS_DIR="$SCRIPT_DIR/../uSockets"

# Step 1: Clone MTEngineSDL if not present
if [[ ! -d "$MTENGINE_DIR" ]]; then
    echo -e "\e[94mCloning \e[31mMTEngineSDL \e[94mlibrary repository\e[0m"
    git clone --recursive https://github.com/slajerek/MTEngineSDL.git "$MTENGINE_DIR"
else
    ( cd "$MTENGINE_DIR" && git submodule update --init --recursive )
fi

# Step 1b: MTEngineSDL ships a stub other/lib/uSockets/ (only Makefile.linux, no sources).
# MTEngineSDL's build-usockets.sh skips cloning when the dir exists, then fails because
# there is no plain Makefile. Replace the stub with the full upstream source before building.
MTENGINE_USOCKETS="$MTENGINE_DIR/other/lib/uSockets"
if [[ ! -f "$MTENGINE_USOCKETS/Makefile" ]]; then
    echo -e "\n\e[94mPopulating \e[31mMTEngineSDL/other/lib/uSockets \e[94mwith upstream source\e[0m"
    rm -rf "$MTENGINE_USOCKETS"
    git clone https://github.com/uNetworking/uSockets.git "$MTENGINE_USOCKETS"
fi

# Step 2: Build MTEngineSDL and all its dependencies
echo -e "\n\e[94m=== Building MTEngineSDL ===\e[0m"
bash "$MTENGINE_DIR/build-linux.sh"

# Step 3: Clone uSockets if not present
if [[ ! -d "$USOCKETS_DIR" ]]; then
    echo -e "\n\e[94mCloning \e[31muSockets \e[94mlibrary repository\e[0m"
    git clone https://github.com/uNetworking/uSockets.git "$USOCKETS_DIR"
fi

# Step 4: Build uSockets and stage for MTEngineSDL
echo -e "\n\e[94m=== Building uSockets ===\e[0m"
( cd "$USOCKETS_DIR" && make -j"$(nproc)" )
mkdir -p "$MTENGINE_DIR/platform/Linux/libs/"
cp -f "$USOCKETS_DIR/uSockets.a" "$MTENGINE_DIR/platform/Linux/libs/"

# Step 5: Build MTEngineSDLDummyApp
echo -e "\n\e[94m=== Building MTEngineSDLDummyApp ===\e[0m"
mkdir -p "$SCRIPT_DIR/build"
cd "$SCRIPT_DIR/build"
cmake ../ ${CMAKE_EXTRA_ARGS:-}
make -j"$(nproc)" MTEngineSDLDummyApp

if [[ -f "$SCRIPT_DIR/build/MTEngineSDLDummyApp" ]]; then
    echo -e "\n\e[1;92mMTEngineSDLDummyApp compiled successfully. Binary is in ./build folder.\e[0m"
else
    echo -e "\n\e[1;91mFailed to build MTEngineSDLDummyApp.\e[0m"
    exit 1
fi
