# Changelog

## Version numbers

This template carries the version of the engine it is built against, and the
same convention: **odd minor versions are development, even minor versions are
stable.**

- `3.21`, `3.23`, … live on `devel`, alongside the engine's `devel`.
- `3.22`, `3.24`, … live on `master`, alongside the engine's `master`. A stable
  release is made by merging `devel` into `master` and bumping to the next even
  number, once it has been built and tested on macOS, Linux and Windows.

`MTENGINE_REF` names the engine revision to build against and moves with the
branch: `origin/devel` here on `devel`, `origin/master` on `master`.

**This template and the engine carry the SAME number.** They are a matched pair
and are read as one release, so a reader who has 3.21.6 of one should not have
to work out which of the other goes with it.

**The engine mints the number; this template only carries it.** A change here
alone gets no number of its own — it ships under whatever number the engine
currently has, however many template releases have landed on that engine. A new
number appears in the engine first, and this repository follows it, which is
why entries below read "No change in this repository. The number moves with
engine X". This template is the engine's, not the other way round, and the rule
is its own: an application built on the engine versions itself however it
likes.

The convention starts at 3.21.

---

## 3.21.15 — development

**A development build no longer packages, and tests run from the git root.**
`./build-<os>.sh` now produces the binary and nothing else; `--prod` is the
final build, which makes `platform/<P>/prod/<arch>/` and is verified from it
with `tests/run_test.sh --package`. The runner's habit of copying fixtures into
the package is gone -- it would have been 17 GB for one sibling app -- and
fixtures are found through the engine's new `CTest::ResolveProjectPath()`,
which works from the root and from the package alike. The Xcode scheme and
the Visual Studio project now start the app in the git root, where its assets
are. The procedure, for every app on this engine, is
`MTEngineSDL/docs/testing.md`.

**The editor marks fxc's error lines.** d3dcompiler_47 prefixes each
diagnostic with a source name — the current directory plus `Shader@0x…` when
it was given none, as in `C:\app\Shader@0x…(41,12-20): error X3004: …` — and
the line-number rebasing only accepted a parenthesis at the start of the
line, so under D3D11 no line was ever marked and every number shown was the
preamble-offset one. The parser now ignores the prefix, which may contain
spaces, and keys on `(line,col` followed by `): error` or `): warning`;
`CTestShaderToyDemo` feeds it every driver's format on every platform rather
than only the running one.

**Full screen on the shader output window no longer stops the app.** Not a
shader bug and nothing in this repository was wrong: `CGuiView` pushed the
fullscreen style vars for the fullscreen view and popped them for *every*
view still rendering, and this app's main view — which uses the engine's
`PreRenderImGui`/`PostRenderImGui` pair, as a template app should — is the one
view that keeps rendering while another is fullscreen. It popped two style
vars it had never pushed, every frame, and ImGui's "Calling PopStyleVar() too
many times!" stops the process under a debugger. Fixed in the engine; the
regression test is here, since this is the app that exposes it.

Two things the test itself had to learn, both recorded in its comments.
`guiMain->SetViewFullScreen` **cannot be called from an imgui_test_engine
TestFunc**: it takes `guiMain`'s mutex while the render thread is inside the
frame holding it, and the suite hung for its full 120-second timeout. And a
window that is laid out for a desktop-sized main window can sit entirely
outside the headless viewport, where a click at its centre is clamped away —
so the test moves it into view first.

## 3.21.14 — development

**Shader Toy has four texture channels**, `iChannel0..3`, as a strip of four
thumbnails under the editor. **The thumbnail is the button**: click it and the
platform's file dialog opens for that slot; right-click for "font atlas" and
"clear". Each slot keeps its own filter (linear/nearest) and wrap
(repeat/clamp), and all three are remembered between runs. Channel 0 starts on
ImGui's font atlas — the one texture that exists on every backend with nothing
to load — so the Texture preset shows something on a first run rather than a
black rectangle.

**Loaded images are no longer upside down**, and the cause was not how the
engine loads them. `CSlrImage` stores images top-down, the way ImGui wants
them — which is why the thumbnails in the panel are upright. ShaderToy's
`fragCoord` counts from the **bottom**, so `uv = fragCoord / iResolution.xy`
hands `v = 0` to the bottom of the screen while `v = 0` is the top row of the
texture. shadertoy.com has the same problem and the same answer: a per-channel
**vflip**, defaulted on, which is now a **Flip Y** checkbox on each slot.

The dialog offers **everything the engine decodes**, read off `CImageData`'s
own extension dispatch rather than guessed: KTX2, TIFF, WebP, HEIC/HEIF, AVIF,
PNG, nine RAW formats through their embedded preview, and the stb_image chain
(JPEG, BMP, TGA, PSD, GIF, HDR, PIC, PNM).

Sample them with `texChannel0(uv)` .. `texChannel3(uv)`, which spell out the
same call on all three backends and apply the wrap mode. **Wrapping is done in
the shader, not by the sampler**: the engine pads every texture up to a power
of two, so hardware repeat would tile the padding instead of the image.

**Every preset now opens with `MAIN_IMAGE`.** With four channels MSL's
`mainImage` takes eleven parameters while GLSL's and HLSL's take two, so each
backend defines the macro as its own signature and a preset writes one word.
The three language variants of a preset now differ only in the arithmetic
spellings, which was always the point of writing them out three times.

**Texture** is aspect-corrected through `iChannelResolution[0]` and has lost
its `fract()`, so the wrap combo actually does something. **Two Channels** is
new: `iChannel1` displaces `iChannel0`, which is the one thing a single-channel
example cannot show.

**The uniform block is complete.** `iFrameRate`, `iDate`, `iSampleRate` and
`iChannelTime` were shipping as zero, which in a shader does not read as
missing — it reads as a plausible lie.

**Compile errors moved under the editor**, where a compiler's output belongs —
above it, every new error pushed the code down the screen. The failing lines
are now banded in red in the editor itself, with the driver's own message as
the tooltip.

**The error text can be selected and copied**, and each diagnostic is its own
block with a rule between them instead of one wall of red. A `Copy` button
takes the whole log in one click.

**Shader Toy now has two windows**, and the shader in it is no longer flat.

The editor and the running shader are separate windows. One window meant the
shader was permanently a strip beneath a text box, could not be enlarged
without shrinking the editor, and could not go fullscreen. Right-clicking the
output window offers **Full screen**, which is the engine's own view-fullscreen
— other views hidden, aspect ratio kept.

**The Tunnel preset drew one flat pulsing colour instead of a tunnel**, and the
shader was not at fault. The quad was drawn with `AddRectFilled`, which gives
all four vertices the font atlas' white-pixel UV, so every fragment computed
the same coordinate and only `iTime` still varied. It is drawn with `AddImage`
now, whose UVs interpolate. A regression test measures the UV span of the quad
ImGui is actually handed, and fails if it ever collapses again.

## 3.21.13 — development

**A real code editor.** `Examples > Shader Toy` no longer edits in a plain
text box: it uses the engine's newly vendored ImGuiColorTextEdit, with syntax
highlighting in the language of the running backend — GLSL, HLSL, or the C++
definition under Metal, since MSL is C++14. Selection works, undo works,
Ctrl+F finds. Compile moved from Ctrl+Enter to **Alt+Enter**, which is what
shadertoy.com uses, with F5 as an alias; the editor binds Ctrl+Enter itself.

**`Examples > Code Editor`** — the engine's `CGuiViewCodeEditor` in a window,
with the same widget and a toolbar the app draws through the wrapper's one
extension point.

**A font of your own.** Both editors offer a font combo — the default UI font,
the engine's JetBrains Mono, or **Courier Prime Code, embedded by this app** —
and the choice is one shared setting. `src/Fonts/` is the worked example of
shipping a font inside a single executable: compressed data, loader, and the
licence declaration in `mtengine-app-licenses.json`, which is new and is how
an application now gets its own dependencies into the `LICENSES.txt` it ships.

## 3.21.12 — development

**`Examples > Shader Toy` — a live fragment-shader editor.** Type
`mainImage()`, press Ctrl+Enter, and watch it run. It is the worked reference
for the engine's new `CreateCustomFragmentShader` seam, and it is the answer to
a question this template could not previously answer: how do I ship my own
shader?

The editor speaks **the language of whatever backend is running** — GLSL under
OpenGL, MSL under Metal, HLSL under D3D11 — and each preset is written out in
all three, which makes the differences between them the example's subject
rather than a footnote. The whole list of differences turns out to be short:
`vecN` becomes `floatN`, `mod` becomes `fmod`, `atan(y,x)` becomes `atan2`,
`mix` becomes `lerp`, and MSL carries the uniform block as a third parameter
because it has no bound program-scope globals. A transpiler would have hidden
all of that at the price of two large engine dependencies.

Two presets ship: a kaleidoscopically folded **Tunnel** with a cosine palette,
and a ten-line **Hello UV** whose only job is that the three languages fit on
one screen together.

Details that matter when you use it:

- **A broken shader does not blank the preview.** The build happens on a fresh
  program and swaps in only on success, so the last shader that worked stays on
  screen while you fix the typo.
- **Error line numbers point at your line.** Each backend prepends a preamble
  the editor does not show, so the reported number is rebased before display.
  Four compiler formats are handled, including Mesa's — the one CI runs and no
  developer's desktop shows.
- **Your text survives a restart**, stored per language beside `settings.hjson`
  so a GLSL draft is never handed to the Metal compiler after a backend switch.

`CTestShaderToyDemo` drives the whole cycle on whatever backend is running: the
preset compiles, a deliberately broken source is reported as broken *with the
driver's own words*, the previous shader survives it, and valid source
recovers. Plus a UI test that opens it from the menu and compiles through the
button.

## 3.21.11 — development

Five new examples, each a small self-contained view over one engine mechanism
that had no worked reference in this template before:

- **Undo & Redo** over `CUndoManager` — a text buffer with an undo stack, so
  the two-list model and what a command has to store are visible in one file.
- **Gamepad Viewer** over `GAM_EnumerateGamepads` — live axes, buttons, and
  hot plug/unplug for every connected pad.
- **Terminal** over `CGuiViewTerminal` — the engine's terminal widget with a
  local echo, the minimum needed to see how input reaches it.
- **Crash Reporter** — writes a synthetic report with
  `MT_CrashReporter_WriteTestReport()`, then shows the native OS dialog through
  `MT_CrashReporter_SpawnHelper()`. It has no view of its own, and that is the
  example: the helper runs as a child process and the running application is
  unaffected. The reporter is installed for every host by the engine's own
  startup, so a host needs no call of its own to get it.
- **File Downloader** over `CFileDownloader` — an HTTP fetch with progress,
  covered by an integration test that downloads from a server started inside
  the test rather than reaching the network, so the suite stays offline and
  deterministic.

Between them they show the two shapes an example can take: wrapping an engine
view that already exists (Terminal), and owning a small view over an engine
mechanism that has none (the other three). None needed a new capability flag,
and the UI tests cover the new menu entries.

From the engine at this number: `MT_VERSION_STRING` had read 3.19 since that
release, so the startup banner, the UI debug view and every crash report's
version field were a release and a half out of date. It now reads 3.21 and
carries the minor only — the compile date and time sit beside it in all three
places and are what actually separate two builds.

New: `docs/getting-started.md`, a checklist for turning a fork of this template
into your own application — what to rename first, what is not worth renaming,
how to run each of the two test suites and why they run from the release
package, and where the engine's own documentation takes over.

---

## 3.21.10 — development

No change in this repository. The number moves with engine 3.21.10, which puts
the Linux diagnostic logging back off now that it has found what it was turned
on for, and teaches the capability test suite to check that the `bash` it
measures with is a real one — on Windows, `bash` on PATH can be the WSL
launcher, which exits 1 while writing its complaint to stdout and so reads as a
failure of the thing being measured.

---

## 3.21.9 — development

No change in this repository. The number moves with engine 3.21.9, which fixes
the crash that stopped this template's own test suites from running on a machine
with no MIDI sequencer: RtMidi's ALSA backend faults inside libasound rather
than failing cleanly when `/dev/snd/seq` is absent, so the engine now asks
before opening one.

3.21.8 is absent here for the same pairing reason — it was an engine-only
diagnostic build.

---

## 3.21.7 — development

**The Linux CI job runs the suites under a virtual display.** They were run
with `--headless`, which in this engine means "do not SHOW the window" — not
"do not need a display". Normal mode still calls `SDL_Init(SDL_INIT_VIDEO)` and
creates a real, hidden OpenGL window and context; only service mode skips that,
and `--headless` does not select it. On a machine with no display server at all
`SDL_CreateWindow` returns NULL, the engine logs "This is fatal!" to its log
*file* and returns, and the assertion on the ImGui context that follows aborts
the process — which is why the first run to get this far produced nothing but
"Aborted (core dumped)" and no results file.

Every previous run of these suites, on any platform, happened on a machine with
a real desktop session, so the gap was never exercised. `xvfb-run` now wraps the
test step, which is enough for Mesa's software GL to give SDL a real, if
invisible, window.

Follows engine 3.21.7, which makes the Windows app build follow the engine's own
toolset instead of each project quietly picking its own.

---

## 3.21.6 — development

**The Linux job installs `libheif-dev`.** libheif is deliberately a system
dependency there — unlike TIFF, WebP, AVIF and LibRaw it is not one of the
archives the engine builds from source — so the package list is the only place
it can come from, and it was never there.

Linux is the only platform that needs it. macOS and Windows decode HEIF through
ImageIO and WIC, and engine 3.21.5 makes the capability system say so, rather
than compiling a translation unit those platforms never reach.

---

## 3.21.5 — development

**The Linux job installs `nasm`.** libvpx needs an x86 assembler on an x86
runner, and has no assembler-less fallback; without it the codec build stops at
"Neither yasm nor nasm have been found". This is the native architecture there,
not a universal-build extra.

**The Windows job installs MSYS2's `make`.** FFmpeg is configured out of tree
and its generated Makefile includes the source Makefile by absolute MSYS2 path
(`/c/Users/...`); only an MSYS2 `make` can read that. A native Windows GNU make
reads `/c/Users` as `C:\c\Users` and stops with `No rule to make target`, naming
FFmpeg's own source Makefile — which is how run `33742482983` failed, looking
exactly like a broken extraction. libvpx passed in the same shell because its
generated Makefile includes `config.mk` relatively.

Engine 3.21.4 makes the script call `/usr/bin/make` by absolute path, so PATH
order can no longer decide this. What remains is the prerequisite itself: `make`
is not part of a base MSYS2 install, no runner image is contracted to carry it,
and when it is missing PATH search does not fail — it falls through to whatever
other make the machine has. The new step installs it if absent (`pacman -S
--needed`, a no-op otherwise) and prints both makes' versions, whose second line
— `Built for x86_64-pc-msys` against `Built for Windows32` — is the
discriminator. The same step is in all four app repositories, since all four run
the same engine script.

Follows engine 3.21.4.

---

## 3.21.4 — development

**The macOS job installs nasm.** macOS builds the video codecs universal —
arm64 and x86_64 — and the x86_64 half needs an x86 assembler. libvpx has no
assembler-less fallback the way FFmpeg's `--disable-x86asm` is, so on a runner
without one its configure stops at "Neither yasm nor nasm have been found",
inside an Xcode script phase where the message is nearly unfindable. A developer
machine that has nasm from Homebrew or MacPorts never sees this.

ARM is unaffected and always was: aarch64 NEON reaches the compiler as
intrinsics in libvpx and as `.S` files assembled by clang in FFmpeg. nasm is an
x86-only requirement.

Follows engine 3.21.3.

---

## 3.21.3 — development

**The Linux job installs `libxss-dev`.** SDL3's X11 backend checks for
XScreenSaver and the workflow's package list never carried it, so the first
continuous-integration run of `devel` stopped at SDL3's configure step. It had
not surfaced before because the workflows only ever ran on `master` and `main`.

Follows engine 3.21.2, which carries the two build fixes the same run found.

---

## 3.21.2 — development

Continuous integration only; no change to what is built.

**The workflows run on `devel`.** They triggered on `master` and `main` alone,
so under the odd/even convention — where `devel` is the branch work lands on and
`master` only ever receives a merge that `devel` has already proved — CI ran
after the question had been answered rather than while it was open. A `devel`
branch could therefore be published green and untested, which is what happened
with 3.21.1.

**The Windows job's `--binary` path is written with forward slashes.** It is
handed to bash, which reads a backslash as an escape, and the test runner now
resolves that argument to an absolute path through `dirname` and `basename` —
neither survives `.\a\b`. Windows accepts forward slashes wherever it accepts a
path.

---

## 3.21.1 — development

Follows engine 3.21.1. Three changes, all in how this template is built and
tested rather than in what it demonstrates.

**The UI scale is detected on first run.** The application started at scale 1.0
whatever the display, which on a HiDPI screen meant it drew at a fraction of the
size of every other window. SDL3 declares per-monitor DPI awareness, so on
Windows and Linux an application is handed real physical pixels and nothing
scales unless it asks; macOS is the opposite, and scaling again there would
double. `MT_DetectDisplayUiScale()` in the engine knows that asymmetry, and the
detected value is passed as the *default* of the config read — which is exactly
"detect on first run, honour the user's choice ever after", with no second
setting to keep in step. Headless runs are pinned to 1.0 before the config is
read, so a scale left behind by an interactive session cannot change what the
suites measure.

**Tests run from the release package.** An engine application resolves its
assets through the current working directory and never through the location of
its executable, so running the suite from the repository root only ever worked
for an application whose repository root happens to contain `assets/`. The
runner now runs from `platform/<Platform>/prod/<arch>/`, the package the build
produces, and makes the results path absolute — as a relative path it silently
wrote nothing from inside the package, and the previous run's verdict was
reported in its place. `MT_TEST_RUN_DIR` pins the directory, and with no package
present the runner falls back to the repository root and says so.

**Visual Studio builds no longer collide with the command-line build.** Both
wrote the executable to the same path while using different intermediate
directories, so each could see the other's output as newer than its own objects
and skip linking — a command-line build could report success and package a
binary it had not produced. The IDE build now has its own output directory.

**`Directory.Build.props` became a stub.** The MSBuild properties an application
needs are the engine's now (`MTEngineApp.props`); this file declares this
application's identity and imports them, which is what the targets file next to
it already did.

---

## 3.21 — development

The build became a set of parameters over an engine-owned flow.

**Three thin stubs.** `build-macos.sh`, `build-linux.sh` and
`build-windows.ps1` clone the engine when it is absent and hand over to its
app-build driver. The flow — resolve, dependency acquisition, engine build,
app build, licence gate, symbols — lives in the engine and is the same for
every app built on it. What this repository holds instead is what makes it
this app:

    mtengine.caps        which engine capabilities to compile in
    mtengine-app.conf    project, scheme, target and executable names
    MTENGINE_REF         the engine revision to build against

**Capabilities.** The manifest names what the app needs — video playback,
photo codecs, LLM inference, HTTPS, websockets, MIDI, the terminal, the test
engine. The build turns that into compiler defines, the dependencies to
acquire and the licences to ship; a capability that is off costs nothing.
`--set KEY=VALUE` (`-Set` on Windows) overrides one for a single build without
editing the manifest.

**Nothing a build produces is written inside this checkout**, or the engine's.
Objects, dependency archives, generated headers and binaries live under a
cache root outside both.

**Visual Studio builds the same binary the command line does**: the IDE reads
the capability set through the engine's MSBuild targets, imported here in a
short `Directory.Build.targets` rather than copied.

**Tests.** Two suites, both headless: `CTestSuite` covers app startup, fonts,
i18n and the HDR test view; `imgui_test_engine` drives the UI. CI builds on
all three platforms and runs both, plus the capability system's own tests.

## Earlier

The repository tracked the engine without a version of its own. See the commit
history.
