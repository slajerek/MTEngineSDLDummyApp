# Starting a new app from this template

Audience: you cloned/forked `MTEngineSDLDummyApp` to build your own product on
MTEngineSDL, and you want a working dev environment plus a checklist for
turning "the dummy app" into "my app".

Everything here is either a required first step or a pointer to a doc that
already explains the thing properly. Nothing is repeated that's covered
better elsewhere.

## 0. Prerequisites

- MTEngineSDL as a **sibling checkout** — `../MTEngineSDL` next to this repo.
  You don't have to clone it yourself: every `build-*` script clones it
  automatically on a fresh checkout (see `MTENGINE_REF` in step 3).
- macOS: Xcode. Linux: CMake + a C++20 toolchain. Windows: Visual Studio
  2019/2022.
- git.

## 1. Prove the environment works before changing anything

```bash
./build-macos.sh      # or
./build-linux.sh      # or
.\build-windows.ps1
```

This clones/builds MTEngineSDL against this app's `mtengine.caps`, then builds
the app itself. First build is slow (it builds the engine and its bundled
dependencies); later ones are incremental.

Then run the tests. There are two suites: async integration tests
(`--run-suite`) and ImGui UI automation (`--run-tests`). On macOS one wrapper
builds, runs and parses both:

```bash
tests/run_test.sh
```

Elsewhere, run the binary **from the release package** the build produced —
`platform/<Platform>/prod/<arch>/`, which holds the binary beside its
`assets/`. This matters: an engine app resolves assets through the current
working directory and never through the executable's own location, so a run
from anywhere else starts without them.

```bash
cd platform/Linux/prod/x86_64        # or platform/Windows/prod/ARM64, etc.
./MTEngineSDLDummyApp-nc --headless --log-dir /tmp \
    --results-file ../../../../tests/results/last_run.txt \
    --run-suite --exit-after-tests
```

`--headless` is required for automated runs. Pass `--results-file` (or set
`MT_TEST_RESULTS`) whenever you run from the package: the default results path
is relative to the working directory, and the file is opened without creating
directories, so without it the run writes nothing and a parser reads the
*previous* verdict.

If this doesn't pass on a clean clone, fix that before writing any app code —
you want to know a red test means *you* broke something, not that the
template arrived broken.

## 2. Give the app its identity

The minimum to stop it being visibly "DummyApp":

| What | Where |
|---|---|
| Window title | `MT_GetMainWindowTitle()` in `src/DummyInit.cpp` |
| Settings/config folder name | `MT_GetSettingsFolderName()` in `src/DummyInit.cpp` — changing this after users have run the old name means they lose their saved settings, irrelevant on a fresh fork |
| Default window size/position | `MT_GetDefaultWindowPositionAndSize()`, same file |
| macOS bundle copyright / camera-permission string | `platform/MacOS/Info.plist` (`NSHumanReadableCopyright`, `NSCameraUsageDescription` — only matters if you keep the Camera example) |
| UI strings | `assets/locale/{en,pl,it}.json` — drop the languages you don't want, or add more; see MTEngineSDL's `docs/i18n.md` |
| `README.md` | rewrite it, the current one describes the template |

**Not required, and don't bother yet:** the internal C++ class names
(`CViewDummyAppMain`, `CDummyAppTestSuite`, `CDummyAppI18n`, …) and the
Xcode/CMake/Visual Studio project identities (`MTEngineSDLDummyApp` as a
target/scheme/solution name). They're pure naming convention with zero
functional effect — see step 4 if you want them renamed too, later.

Point git at your own remote (`git remote set-url origin <your-repo>`, or
start a fresh history) once you're ready to stop tracking this template.

## 3. Decide what you actually need

- **Capabilities** — `mtengine.caps` ships with everything on, because this
  app is also the engine's reference/matrix host. Turn off what you don't
  need (`MT_CAP_X=0`, rebuild) — the file's own header explains the format,
  the implication rules, and the per-OS override syntax. Full vocabulary:
  MTEngineSDL's `docs/CAPABILITIES.md`.
- **Engine version** — `MTENGINE_REF` defaults to tracking the engine's head.
  Pin it to a SHA or tag once you're cutting something that has to stay
  reproducible; the file explains the branch-vs-SHA behavior.
- **Examples menu** — each wired-in example (Music Player, AI/LLM, Camera,
  Video Player, HDR Test, Undo/Redo, Gamepad Viewer, Terminal, Crash Reporter,
  File Downloader, …) is a self-contained view under `src/Views/` plus a menu
  entry, so each is a readable wiring reference for one engine subsystem. Keep
  whichever ones you're actually going to use; delete the rest — removing one
  means pulling its lines from `CViewDummyAppMain.h/.cpp`, `CMainMenuBar.cpp`,
  and — if it added its own source files — all **three** build systems (step 5).

## 4. Optional, later: rename the project identity itself

Skip this until you actually need it — it's mechanical, invasive, and buys
you nothing functionally. When you do:

- Xcode: rename the target, scheme, product name and bundle identifier in
  `platform/MacOS/MTEngineSDLDummyApp.xcodeproj` (Xcode's own **File ▸
  Rename…** handles most of this; the `.xcconfig` files don't need touching).
- Windows: rename the `.sln`/`.vcxproj`, the project, and the output `.exe`.
- CMake: the `project()`/`add_executable()` name in `CMakeLists.txt`.
- `mtengine-app.conf`: `MT_APP_NAME`, `MT_MACOS_PROJECT`, `MT_MACOS_SCHEME`,
  `MT_CMAKE_TARGET`, `MT_WINDOWS_SLN`, `MT_WINDOWS_EXE` — this is what the
  engine's build drivers use to find and invoke your project, so it has to
  agree with whatever you renamed above.
- The repo directory name itself, if you want `./build-*` scripts' relative
  `../MTEngineSDL` sibling assumption to keep working, just keep the new
  directory a sibling of MTEngineSDL too.

## 5. Keep the discipline

Three rules. They're short, and they apply to your app exactly as much as they
applied to the template:

1. **It compiles on all three platforms.** None of the three build systems
   auto-discovers sources — all use explicit file lists — so adding, removing
   or renaming a source file means updating `CMakeLists.txt`,
   `platform/MacOS/MTEngineSDLDummyApp.xcodeproj/project.pbxproj`, and
   `platform/Windows/MTEngineSDLDummyApp/MTEngineSDLDummyApp.vcxproj` **plus**
   its `.vcxproj.filters`, in the same commit. Letting them drift apart is the
   single easiest way to lose a day.
2. **All tests pass.** If they passed before your change, they pass after it.
3. **No new compiler warnings.**

## 6. Where to go next

- `src/Views/` and `src/Tests/` in this repo — the wiring pattern for
  everything in the Examples menu, plus fonts, i18n, settings and the HDR
  bench, and how a test is registered in either framework.
- MTEngineSDL `docs/CAPABILITIES.md` — the full capability vocabulary.
- MTEngineSDL `docs/i18n.md`, `docs/render-backends.md`, `docs/netgame.md`,
  `docs/llama_cpp.md` and its platform variants — whichever subsystems your
  `mtengine.caps` turns on.
