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
