# MTEngineSDL API Reference

MTEngineSDL is the engine that MTEngineSDLDummyApp is built on. It lives at `../MTEngineSDL`.

**IMPORTANT: Never modify MTEngineSDL without explicit user approval.**

## MT_API.h — Functions every app must implement

Located at: `MTEngineSDL/src/MT_API.h`

| Function | When called | Notes |
|----------|-------------|-------|
| `MT_GetMainWindowTitle()` | before window creation | return a `const char*` window title |
| `MT_GetSettingsFolderName()` | early init | folder name for settings persistence (under ~/Library/Application Support on macOS) |
| `MT_GetDefaultWindowPositionAndSize()` | before window creation | sets initial x/y/w/h and maximized flag |
| `MT_PreInit()` | before SDL/GPU init | earliest hook; prefer `MT_PostInit` for most setup |
| `MT_GuiPreInit()` | after SDL, before ImGui | install custom ImGui fonts or style here |
| `MT_PostInit()` | after full engine init | **create views here**, parse CLI flags, start test runners |
| `MT_Render()` | every frame | drive test runner ticks; SDL renderer path (usually no-op if using ImGui) |
| `MT_PostRenderEndFrame()` | after ImGui render + swap | call `CImGuiTestEngine::PostSwap()` here |
| `MT_Shutdown()` | on exit | cleanup; called before process ends |
| `ImPlotColorsExtensionGetterCallback(int)` | implot render | return 0 if not using custom plot colors |

## Key global objects

| Global | Type | Declared in | Notes |
|--------|------|-------------|-------|
| `guiMain` | `CGuiMain*` | `CGuiMain.h` / `GUI_Main.h` | Main GUI controller. Set by engine before `MT_PostInit`. |

## Key headers

| Header | Path in MTEngineSDL | Purpose |
|--------|---------------------|---------|
| `CGuiMain.h` | `src/Engine/GUI/CGuiMain.h` | `guiMain` object, `SetView()`, layout management |
| `GUI_Main.h` | `src/Engine/GUI/GUI_Main.h` | Convenience umbrella: includes `CGuiMain.h` + SYS headers + ImGui |
| `CGuiView.h` | `src/Engine/GUI/` | Base class for all ImGui views; override `RenderImGui()` |
| `SYS_Defs.h` | `src/Engine/Core/` | Primitive types: `u8`, `u32`, `i32`, `STRALLOC`, etc. |
| `SYS_Main.h` | `src/Engine/Core/` | `SYS_Shutdown()`, `SYS_Sleep()`, `SYS_FatalExit()` |
| `SYS_CommandLine.h` | `src/Engine/Core/` | `sysCommandLineArguments` — `std::vector<const char*>` |
| `SYS_KeyCodes.h` | `src/Engine/Core/` | `MTKEY_*` constants for key codes |
| `DBG_Log.h` | platform-specific | `LOGD()` (debug), `LOGM()` (message) logging macros |
| `CTestSuite.h` | `src/Engine/Tests/` | Base class for test suites; `RunFromCLI()` static helper |
| `CTest.h` | `src/Engine/Tests/` | Base class for individual tests; `StepCompleted()`, `TestCompleted()` |
| `CTestRunner.h` | `src/Engine/Tests/` | Single-test runner (used by UI-driven testing) |
| `CImGuiTestEngine.h` | `src/Engine/Tests/` | Wrapper for imgui_test_engine: `Init()`, `PostSwap()`, `QueueAllTests()` |
| `CSlrKeyboardShortcuts.h` | `src/Engine/GUI/Controls/` | Keyboard shortcut system |
| `CMidiInKeyboard.h` | `src/Engine/Audio/MIDI/` | MIDI input callback interface |

## View system

Views inherit from `CGuiView` and override `RenderImGui()`. Set the active view with:

```cpp
guiMain->SetView(myView);   // in MT_PostInit()
```

The current view is accessible as `guiMain->currentView` (public field, type `CGuiView*`).

`PreRenderImGui()` / `PostRenderImGui()` in `RenderImGui()` wrap the ImGui window begin/end.

## Headless mode

Pass `--headless` on the command line to suppress window creation. MTEngineSDL handles this flag itself — the app continues to render ImGui frames internally, making tests possible without a display.

## Logging

`LOGD(fmt, ...)` — debug log (compiled to no-op when `GLOBAL_DEBUG_OFF` is defined).  
`LOGM(fmt, ...)` — message log (always compiled in).

To enable `LOGD` output, comment out `#define GLOBAL_DEBUG_OFF` in:
`MTEngineSDL/platform/MacOS/src.MacOS/DBG_Log.h` (line 33).

Logs are written to `~/Library/Caches/<SettingsFolderName>-*.txt` on macOS by default.  
Pass `--log-dir /tmp` to redirect them (the test runner script does this automatically).

## Build dependency layout

```
../MTEngineSDL/          ← engine source and built library
../uSockets/             ← uSockets (WebSocket transport, required by MTEngineSDL)
MTEngineSDLDummyApp/     ← this repo
```

On Linux: `../MTEngineSDL/build/libMTEngineSDL.a` + `../MTEngineSDL/platform/Linux/libs/uSockets.a`  
On macOS: built by Xcode into DerivedData; `../MTEngineSDL/platform/MacOS/libs/uSockets.a` staged by `build-macos.sh`  
On Windows: `platform/Windows/MTEngineSDL.sln` → `.lib` linked by VS solution
