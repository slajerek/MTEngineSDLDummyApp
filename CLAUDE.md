# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Guidance Files

`AGENTS.md` must remain a symlink to `CLAUDE.md`. Edit `CLAUDE.md` only when changing agent guidance; do not copy/paste divergent guidance into `AGENTS.md`.

## Project Overview

MTEngineSDLDummyApp is a minimal template application for the MTEngineSDL engine. Its purpose is to serve as the canonical starting point when creating new MTEngineSDL-based projects. It demonstrates the MT_API.h lifecycle contract, ImGui view hierarchy, menu bar, MIDI input, and the dual test framework (CTestSuite + imgui_test_engine).

**External Dependency**: Requires MTEngineSDL engine in sibling directory (`../MTEngineSDL`).

**IMPORTANT: Do NOT modify MTEngineSDL directly.** It is an external library. If new functionality is needed in MTEngineSDL, flag this to the user and wait for permission. We will switch to the MTEngineSDL project space to implement changes there. Never implement or change anything in MTEngineSDL without explicit user approval.

If MTEngineSDL changes are explicitly approved, keep MTEngineSDL's own Xcode, CMake, and Visual Studio project files in sync when adding, removing, or renaming source files there.

**IMPORTANT: Do NOT create git worktrees in this repo or in MTEngineSDL. It is not supported.**

## Build Commands

### macOS (Xcode)
```bash
# One-command build (clones deps, builds, produces binary):
./build-macos.sh

# Or open in Xcode directly:
# platform/MacOS/MTEngineSDLDummyApp.xcodeproj
```

### Linux (CMake)
```bash
# One-command build (clones MTEngineSDL + uSockets, builds engine, then app):
./build-linux.sh

# Or manually (MTEngineSDL must already be built):
mkdir -p build && cd build && cmake ../ && make -j$(nproc) MTEngineSDLDummyApp
```

### Windows
```powershell
# PowerShell (from repo root):
.\build-windows.ps1

# From Git Bash / WSL:
./build-windows.sh
```
Or open `platform/Windows/MTEngineSDLDummyApp.sln` in Visual Studio 2019/2022.

### Keeping Build Projects in Sync
**IMPORTANT:** When adding, removing, or renaming source files, you MUST update ALL three build systems together in one commit:
- Xcode: `platform/MacOS/MTEngineSDLDummyApp.xcodeproj/project.pbxproj`
- CMake: `CMakeLists.txt` (the `add_executable()` list)
- Visual Studio: `platform/Windows/MTEngineSDLDummyApp/MTEngineSDLDummyApp.vcxproj` + `.vcxproj.filters`

All three use explicit file lists — there is no auto-discovery.

### Adding Source Files to the Xcode Project

The recommended workflow is to open `platform/MacOS/MTEngineSDLDummyApp.xcodeproj` in Xcode and use **File > Add Files to "MTEngineSDLDummyApp"…** then:
1. Select the new `.cpp`/`.h` files under `src/`.
2. Ensure "Add to targets: MTEngineSDLDummyApp" is checked.
3. Uncheck "Copy items if needed" (files stay in their `src/` location).

If editing `project.pbxproj` directly (e.g. programmatically), you need to touch **four** sections. For each new `.cpp` file:

1. **PBXBuildFile** — one entry per `.cpp` (not `.h`):
   ```
   AABBCCDD2799999900AA0001 /* MyFile.cpp in Sources */ = {isa = PBXBuildFile; fileRef = AABBCCDD2799999900AA0002 /* MyFile.cpp */; };
   ```

2. **PBXFileReference** — one entry per file (both `.cpp` and `.h`):
   ```
   AABBCCDD2799999900AA0002 /* MyFile.cpp */ = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.cpp.cpp; name = MyFile.cpp; path = ../../src/Tests/MyFile.cpp; sourceTree = "<group>"; };
   AABBCCDD2799999900AA0003 /* MyFile.h */  = {isa = PBXFileReference; fileEncoding = 4; lastKnownFileType = sourcecode.c.h; name = MyFile.h; path = ../../src/Tests/MyFile.h; sourceTree = "<group>"; };
   ```
   Paths are relative to the `.xcodeproj` directory (`platform/MacOS/`), so `../../src/...`.

3. **PBXGroup** — add the file refs to the appropriate group's `children` array. The project's `src` group (`41B5299123992DCF0084F0DB`) holds `Views`, `Tests`, and the root source files. Subfolders like `Tests` are their own PBXGroup entries.

4. **PBXSourcesBuildPhase** — add the `.cpp` BuildFile UUID to the `files` array (`.h` files are NOT compiled):
   ```
   AABBCCDD2799999900AA0001 /* MyFile.cpp in Sources */,
   ```

**HEADER_SEARCH_PATHS:** If the new files live in a new subdirectory under `src/`, add the path to `HEADER_SEARCH_PATHS` in both the Debug and Release target build configurations (search for `4147CA9023992D74006B25A7` and `4147CA9123992D74006B25A7`). Use `$(PROJECT_DIR)/../../src/NewDir` as the path format (matches the c64d / RetroDebugger project convention).

**UUID format:** 24 hex characters. Generate unique values — they must not collide with any existing UUID in the file. Xcode does not validate them on open but will complain if two objects share the same UUID.

## Architecture

### MT_API.h Contract
Every MTEngineSDL host must implement these functions in `src/DummyInit.cpp`:

| Function | When called | Notes |
|----------|-------------|-------|
| `MT_GetMainWindowTitle()` | before window creation | window title string |
| `MT_GetSettingsFolderName()` | early init | folder for settings persistence |
| `MT_GetDefaultWindowPositionAndSize()` | before window creation | initial window geometry |
| `MT_PreInit()` | before SDL/GPU init | parse CLI args here if needed early |
| `MT_GuiPreInit()` | after SDL, before ImGui | install ImGui font overrides here |
| `MT_PostInit()` | after full init | create views, start test runners |
| `MT_Render()` | every frame | drive test runner ticks here |
| `MT_PostRenderEndFrame()` | after ImGui render | call `CImGuiTestEngine::PostSwap()` |
| `MT_Shutdown()` | on exit | cleanup |
| `ImPlotColorsExtensionGetterCallback()` | implot render | return 0 if not using custom colors |

### View System
Views inherit from `CGuiView` (in MTEngineSDL). The main view is `CViewDummyAppMain`. Set it with `guiMain->SetView(view)` in `MT_PostInit()`. Render via `RenderImGui()` override.

### Directory Structure
- `src/DummyInit.cpp` — MT_API.h implementation, entry point
- `src/Views/` — `CViewDummyAppMain`, `CMainMenuBar`
- `src/Tests/` — test suite and test implementations
- `platform/MacOS/` — Xcode project
- `platform/Windows/` — Visual Studio solution
- `tests/` — shell runner script and results
- `claude/` — Claude's workspace (technical docs, architecture notes)

### Naming Conventions
- Classes: `C` prefix (e.g., `CViewDummyAppMain`, `CMainMenuBar`)
- Views: `CView*`
- Tests: `CTest*`
- Files: match class name

### File Paths
**NEVER hardcode absolute paths** (`/Users/mars/...`). Always use relative paths or runtime detection.

### C++ Standard
C++20 across all platforms.

### Threading
Use `CSlrMutex` from MTEngineSDL for thread synchronization.

## Logging

Use `LOGD()` / `LOGM()` macros from MTEngineSDL.

To enable output, comment out `#define GLOBAL_DEBUG_OFF` in `MTEngineSDL/platform/MacOS/src.MacOS/DBG_Log.h` (line 33). When the define is active, all log macros compile to no-ops. By default logs are written under `~/Library/Caches`; use `--log-dir /tmp` for automated tests and debugging runs to avoid stale or scattered local cache logs. The `tests/run_test.sh` script already defaults to `/tmp`.

## Testing

Dual-framework test system in `src/Tests/`:

1. **CTestSuite** (`CDummyAppTestSuite`) — Async integration tests. Inherit from `CTest`, use `StepCompleted()` / `TestCompleted()`. Run headlessly.
2. **imgui_test_engine** — UI automation tests registered in `CImGuiDummyTests.cpp`. Requires `ENABLE_IMGUI_TEST_ENGINE` define.

**IMPORTANT: Always use `--headless`** when running automated tests.

### Running Tests

```bash
# Shell wrapper (builds + runs + parses results — macOS only)
tests/run_test.sh                        # All CTestSuite tests
tests/run_test.sh AppStartup             # Single test by name
tests/run_test.sh --skip-build AppStartup

# Direct binary flags
./MTEngineSDLDummyApp --headless --log-dir /tmp --run-suite --exit-after-tests
./MTEngineSDLDummyApp --headless --log-dir /tmp --run-test AppStartup --exit-after-tests
./MTEngineSDLDummyApp --headless --log-dir /tmp --run-tests --exit-after-tests   # ImGui tests
```

Results are written to `tests/results/last_run.txt`.

### Adding Tests

1. Create `src/Tests/CTestMyFeature.h/.cpp` inheriting from `CTest`
2. Register in `CDummyAppTestSuite::RegisterTests()` with `tests.push_back(std::make_unique<CTestMyFeature>())`
3. For ImGui UI tests, add `IM_REGISTER_TEST(...)` calls in `src/Tests/CImGuiDummyTests.cpp`
4. Add new `.cpp` files to all three build systems (CMakeLists.txt, Xcode, VS)

**ALL tests must ALWAYS pass.** After any change, run the suite and verify. Never dismiss a failing test as pre-existing.

Tests should exercise the same production code paths users hit. Do not add test-only shortcuts that bypass real behavior. Environment variables or CLI flags are acceptable for test configuration, output paths, and deterministic fixtures, but not for skipping the feature under test.

## Definition of Done

Work is **not complete** until ALL of the following are true:

1. **Compiles on all three platforms.** Every source file change must be reflected in CMakeLists.txt, the Xcode project, and the VS project before the work is declared done. A change that compiles on Linux/CMake but breaks Xcode or VS is NOT done.
2. **All tests pass.** Run `tests/run_test.sh` (or `--run-suite --exit-after-tests` directly) and verify 100% pass. Never dismiss a failing test as pre-existing or unrelated. If tests passed before your change, they must pass after.
3. **No compiler errors or warnings introduced.** Check the full build output.
4. **All three build system files are consistent.** When adding/removing/renaming source files, CMakeLists.txt, project.pbxproj, and .vcxproj + .vcxproj.filters must all be updated in the same commit.
5. **`claude/` docs are up to date.** If you changed architecture, testing, or the MTEngineSDL API surface, update the relevant doc in `claude/`. Outdated docs are worse than no docs.

**Never say "complete" or sign off on work that hasn't been compiled and tested.**

## Git Commits

- Do NOT add "Co-Authored-By" lines to commit messages.
- When adding/removing source files: update CMakeLists.txt, Xcode, and VS projects in the same commit.
- Before committing: verify the build succeeds and all tests pass.
- **"Commit and push" means**: before committing, review and update the relevant `claude/` documentation, run the required verification, then commit and push this repo. If MTEngineSDL changes were explicitly approved, check, verify, commit, and push the MTEngineSDL repo separately too.

## Claude Workspace (`claude/`)

The `claude/` directory is Claude's dedicated workspace. All Claude-generated artifacts go here — never in project directories like `src/`, `tools/`, or root `docs/` unless the artifact is actual project source code or the user explicitly requests that location.

- `claude/architecture/testing.md` — test framework details and CLI runner
- `claude/external/MTEngineSDL.md` — MTEngineSDL API reference
- `claude/tools/` — scripts and utilities created by Claude

The root `tools/` directory, if present, is reserved for user/project tools, not one-off Claude helpers.

When implementing features, fixing bugs, or changing architecture, testing, or the MTEngineSDL API surface, update the relevant documentation in `claude/`. If no relevant doc exists yet for the area being changed, create one. Outdated docs are worse than no docs.

## Writing Conventions

- **Never use `§` as a section marker.** Use `#` with the section number (e.g., `#1`, `#2.3`). Plain-ASCII portability: `§` renders inconsistently across terminals and grep.
