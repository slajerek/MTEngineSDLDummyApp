# Testing Architecture

MTEngineSDLDummyApp uses the same dual-framework test system as RetroDebugger and LightHeroes.

## Framework 1: CTestSuite (headless async integration tests)

- Base class: `CTest` — in `MTEngineSDL/src/Engine/Tests/CTest.h`
- Suite class: `CDummyAppTestSuite` — in `src/Tests/CDummyAppTestSuite.cpp`
- Tests inherit `CTest`, implement `GetName()`, `Run(ITestCallback*)`, `Cancel()`
- Use `StepCompleted(stepId, success, message)` and `TestCompleted(success, summary)` to report progress
- Results written to `tests/results/last_run.txt` in the format `RESULT: N/M`

### Lifecycle inside `Run()`

```cpp
void CTestMyFeature::Run(ITestCallback *callback)
{
    this->callback = callback;
    isRunning = true;
    int stepNum = 1;

    // each assertion calls StepCompleted or TestCompleted(false) + return
    ASSERT_TRUE(someCondition, "description of what we checked");

    TestCompleted(true, "All steps passed");
}
```

### CLI flags (parsed in MT_PostInit → MT_Render)

```
--headless              suppress window (MTEngineSDL handles this internally)
--run-suite             run all registered tests in CDummyAppTestSuite
--run-test <Name>       run a single test by GetName() value
--exit-after-tests      call SYS_Shutdown() when suite finishes
```

### Adding a CTest test

1. Create `src/Tests/CTestMyFeature.h/.cpp` inheriting `CTest`
2. Register in `CDummyAppTestSuite::RegisterTests()`:
   ```cpp
   tests.push_back(std::make_unique<CTestMyFeature>());
   ```
3. Add the `.cpp` to `CMakeLists.txt` `add_executable()` list
4. Add the `.cpp` to the Xcode project (`platform/MacOS/MTEngineSDLDummyApp.xcodeproj`)
5. Add the `.cpp` to the VS project (`platform/Windows/MTEngineSDLDummyApp/MTEngineSDLDummyApp.vcxproj`)

## Framework 2: imgui_test_engine (UI automation)

- Enabled by `-DENABLE_IMGUI_TEST_ENGINE` compile define (already set in CMakeLists.txt)
- Tests registered in `src/Tests/CImGuiDummyTests.cpp` via `RegisterDummyAppTests(ImGuiTestEngine*)`
- `RegisterDummyAppTests` is declared `extern` in `DummyInit.cpp` and called after `CImGuiTestEngine::Init()`
- `CImGuiTestEngine::PostSwap()` is called each frame from `MT_PostRenderEndFrame()`

### Key macros

```cpp
ImGuiTest *t = IM_REGISTER_TEST(engine, "group", "test_name");
t->TestFunc = [](ImGuiTestContext *ctx) {
    ctx->SetRef("##MainMenuBar");
    ctx->MenuCheck("File");           // verify menu item exists
    ctx->MenuClick("File/Open");      // click a menu item
    ctx->WindowFocus("MyWindow");     // verify window appeared
};
```

### CLI flags

```
--headless --run-tests --exit-after-tests
```

### Adding an ImGui test

Add an `IM_REGISTER_TEST(...)` block in `src/Tests/CImGuiDummyTests.cpp` inside `RegisterDummyAppTests()`.

## Shell runner (macOS)

`tests/run_test.sh` — builds via xcodebuild, finds the binary in DerivedData, runs it with `--headless`, waits with timeout, parses `tests/results/last_run.txt`.

```bash
tests/run_test.sh                        # all suite tests
tests/run_test.sh AppStartup             # single test
tests/run_test.sh --skip-build AppStartup
```

## Current tests

| Name | Framework | Description |
|------|-----------|-------------|
| `AppStartup` | CTestSuite | Verifies `guiMain` is not null and `currentView` is set after init |
| `ui/main_menu_bar_items` | imgui_test_engine | Verifies File, Workspace, Help menus exist in menu bar |
| `ui/file_menu_quit` | imgui_test_engine | Verifies File > Quit item is present |
