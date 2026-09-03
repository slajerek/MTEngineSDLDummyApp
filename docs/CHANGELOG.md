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

The convention starts at 3.21.

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
