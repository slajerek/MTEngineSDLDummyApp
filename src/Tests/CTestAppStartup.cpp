#include "CTestAppStartup.h"
#include "CGuiMain.h"
#include "DBG_Log.h"
#include "CViewDummyAppMain.h"
#include "CGuiViewMusicPlaylist.h"
#include <cstring>
#if MT_CAP_LLM
#include "CViewLlamaTaskQueue.h"
#include "CGuiViewLlamaModelLoader.h"
#include "CGuiViewLlamaChat.h"
#endif
#include "CViewCamera.h"
#include "CViewUndoDemo.h"
#include "CUndoFieldChange.h"
#include "CViewGamepadViewer.h"
#include "GAM_GamePads.h"
#include "CViewTerminalDemo.h"
#include "MT_CrashReporter.h"
#include "CViewFileDownloaderDemo.h"
#include <filesystem>

#define ASSERT_TRUE(cond, msg)                                   \
    do {                                                          \
        if (!(cond)) {                                            \
            char buf[256];                                        \
            snprintf(buf, sizeof(buf), "FAIL: %s", msg);         \
            LOGD("CTestAppStartup: %s", buf);                    \
            TestCompleted(false, buf);                            \
            return;                                               \
        }                                                         \
        StepCompleted(stepNum++, true, msg);                      \
    } while (0)

CTestAppStartup::CTestAppStartup() {}
CTestAppStartup::~CTestAppStartup() {}

void CTestAppStartup::Run(ITestCallback *callback)
{
    this->callback = callback;
    isRunning = true;
    int stepNum = 1;

    LOGD("CTestAppStartup: running");

    // Step 1: guiMain must exist (MTEngineSDL creates it during init)
    ASSERT_TRUE(guiMain != nullptr, "guiMain is initialized");

    // Step 2: main view must be set (MT_PostInit set it via guiMain->SetView)
    ASSERT_TRUE(guiMain->currentView != nullptr, "main view is set");

    CViewDummyAppMain *viewMain = dynamic_cast<CViewDummyAppMain *>(guiMain->currentView);
    ASSERT_TRUE(viewMain != nullptr, "main view is CViewDummyAppMain");

    ASSERT_TRUE(viewMain->viewMusicPlaylist != nullptr, "music playlist example view is created");
#if MT_CAP_LLM
    ASSERT_TRUE(viewMain->viewAiSetup != nullptr, "LLM settings example view is created");
    ASSERT_TRUE(viewMain->viewAiChat != nullptr, "LLM chat example view is created");
#endif
    ASSERT_TRUE(viewMain->viewImageLoader != nullptr, "image loader example view is created");
    ASSERT_TRUE(viewMain->viewCamera != nullptr, "camera example view is created");
    ASSERT_TRUE(viewMain->viewUndoDemo != nullptr, "undo/redo example view is created");
    ASSERT_TRUE(viewMain->viewGamepadViewer != nullptr, "gamepad viewer example view is created");
    ASSERT_TRUE(viewMain->viewTerminalDemo != nullptr, "terminal example view is created");
    ASSERT_TRUE(viewMain->viewFileDownloaderDemo != nullptr, "file downloader example view is created");

    viewMain->OpenExampleMusicPlayer();
    ASSERT_TRUE(viewMain->viewMusicPlaylist->visible, "music playlist example opens the engine view");

    viewMain->OpenExampleUndoRedo();
    ASSERT_TRUE(viewMain->viewUndoDemo->visible, "undo/redo example opens the engine view");

    // Exercise the real CUndoManager/CUndoFieldChange path directly -- no
    // ImGui context is needed to drive it, unlike the ImGuiUndo widgets.
    viewMain->viewUndoDemo->undoMgr.Push(
        std::make_unique<CUndoFieldChange<int>>(&viewMain->viewUndoDemo->demoInt, 0, 42));
    viewMain->viewUndoDemo->demoInt = 42;
    ASSERT_TRUE(viewMain->viewUndoDemo->undoMgr.CanUndo(), "undo history has an entry after a push");
    viewMain->viewUndoDemo->undoMgr.PerformUndo();
    ASSERT_TRUE(viewMain->viewUndoDemo->demoInt == 0, "undo restores the previous value");
    ASSERT_TRUE(viewMain->viewUndoDemo->undoMgr.CanRedo(), "redo is available after an undo");
    viewMain->viewUndoDemo->undoMgr.PerformRedo();
    ASSERT_TRUE(viewMain->viewUndoDemo->demoInt == 42, "redo re-applies the change");

    viewMain->OpenExampleGamepadViewer();
    ASSERT_TRUE(viewMain->viewGamepadViewer->visible, "gamepad viewer example opens the engine view");

    // Hardware-independent sanity check: valid on a machine with zero pads.
    int numGamepads = 0;
    CGamePad **pads = GAM_EnumerateGamepads(&numGamepads);
    ASSERT_TRUE(pads != nullptr, "gamepad enumeration returns a valid array");
    ASSERT_TRUE(numGamepads == MAX_GAMEPADS, "gamepad enumeration returns the fixed slot count");

    viewMain->OpenExampleTerminal();
    ASSERT_TRUE(viewMain->viewTerminalDemo->visible, "terminal example opens the engine view");

    // Round-trip smoke check: exercises the local-echo write callback without
    // a real keypress. CGuiViewTerminal's internal VT100 buffer is protected,
    // so this cannot assert on rendered content -- only that writing does not
    // crash the wiring.
    viewMain->viewTerminalDemo->SendData("hello\n");
    ASSERT_TRUE(viewMain->viewTerminalDemo->visible, "terminal view still visible after a local-echo round trip");

    // Exercise only the safe half of the crash-reporter demo: writing a
    // synthetic report is real production code with no side effect on the
    // running process. MT_CrashReporter_SpawnHelper() is never called here --
    // it launches a subprocess and shows a native dialog, which headless CI
    // must not trigger.
    char crashReportPath[512];
    bool crashReportOk = MT_CrashReporter_WriteTestReport(crashReportPath, sizeof(crashReportPath));
    ASSERT_TRUE(crashReportOk, "crash reporter writes a synthetic test report");
    ASSERT_TRUE(std::filesystem::exists(crashReportPath), "the synthetic crash report file exists on disk");

    viewMain->OpenExampleFileDownloader();
    ASSERT_TRUE(viewMain->viewFileDownloaderDemo->visible, "file downloader example opens the engine view");
    // The actual download exercise (starting the local server, downloading,
    // verifying the file) is a dedicated CTest -- CTestFileDownloaderDemo --
    // since it runs real async I/O on its own timeline rather than the fast
    // synchronous steps this test asserts.

    // BOTH BRANCHES ARE ASSERTED. The capability-aware test shape: compiled-in
    // must work, compiled-out must report unavailable. Never skip silently, and
    // never #ifdef the test out altogether -- a suite that quietly shrinks when a
    // capability is turned off proves nothing about the off path.
#if MT_CAP_LLM
    ASSERT_TRUE(MT_HAS_CAP(MT_CAP_LLM), "MT_HAS_CAP agrees with MT_CAP_LLM (on)");

    viewMain->OpenExampleLlmSettings();
    ASSERT_TRUE(viewMain->viewAiSetup->visible, "LLM settings example opens the engine view");

    viewMain->OpenExampleLlmChat();
    ASSERT_TRUE(viewMain->viewAiChat->visible, "LLM chat example opens the engine view");

    CViewLlamaTaskQueue::sOpen = false;
    viewMain->OpenExampleLlmTasks();
    ASSERT_TRUE(CViewLlamaTaskQueue::sOpen, "LLM tasks example opens the engine task queue");
#else
    // The off path has its own assertions rather than an absence of them.
    ASSERT_TRUE(!MT_HAS_CAP(MT_CAP_LLM), "MT_HAS_CAP agrees with MT_CAP_LLM (off)");
    ASSERT_TRUE(MT_CAP_DEFINED_MT_CAP_LLM,
                "MT_CAP_LLM is a KNOWN capability that is off, not an unknown one");

    // The manifest is the single source of truth, so the string this binary
    // reports must agree with the macro the same header defined.
    ASSERT_TRUE(strstr(MT_GetCapabilityManifest(), "MT_CAP_LLM=0") != nullptr,
                "the resolved manifest string records MT_CAP_LLM=0");
#endif

    viewMain->viewCamera->visible = true;
    viewMain->PrepareForQuit();
    ASSERT_TRUE(viewMain->viewCamera->visible == false, "pre-quit cleanup hides camera example view");

    LOGD("CTestAppStartup: all steps passed");
    TestCompleted(true, "App initialized correctly");
}

void CTestAppStartup::Cancel()
{
    isRunning = false;
}
