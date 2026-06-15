#!/bin/bash
# Build MTEngineSDLDummyApp on Windows from Git Bash / MSYS2 / WSL.
# Thin wrapper around build-windows.ps1 — pass the same flags.
#
# Usage: ./build-windows.sh [options]
#   -Platform <x64|ARM64>
#   -Configuration <Release|Debug>
#   -Compiler <Clang|MSVC>
#   -Clean
#   --help, -h

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    sed -n '2,/^$/{ s/^# \?//; p; }' "${BASH_SOURCE[0]}"
    exit 0
fi

if command -v pwsh.exe &>/dev/null; then PS=pwsh.exe
elif command -v powershell.exe &>/dev/null; then PS=powershell.exe
elif command -v pwsh &>/dev/null; then PS=pwsh
elif command -v powershell &>/dev/null; then PS=powershell
else echo "Error: PowerShell not found. Install PowerShell or use build-windows.ps1 directly." >&2; exit 1
fi

PS1_SCRIPT="$SCRIPT_DIR/build-windows.ps1"
command -v cygpath &>/dev/null && PS1_SCRIPT="$(cygpath -w "$PS1_SCRIPT")"

exec "$PS" -ExecutionPolicy Bypass -File "$PS1_SCRIPT" "$@"
