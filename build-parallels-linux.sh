#!/bin/bash
# Build for Parallels VM where host CPU features aren't available to the guest.
# Disables native CPU optimizations in ggml/llama.cpp inside MTEngineSDL.
export CMAKE_EXTRA_ARGS="-DMT_GGML_NATIVE=OFF"
exec "$(dirname "$0")/build-linux.sh"
