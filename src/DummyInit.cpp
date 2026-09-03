#include "DummyInit.h"
#include "DBG_Log.h"
#include "CGuiMain.h"
#include "MT_API.h"
#include "SYS_CommandLine.h"
#include "CViewDummyAppMain.h"
#include "CDummyAppTestSuite.h"
#include "CTestRunner.h"
#include "CDummyAppI18n.h"
#include "MT_UiScale.h"        // MT_DetectDisplayUiScale / MT_SetUiScale
#include "SYS_DefaultConfig.h"   // gApplicationDefaultConfig

#if MT_ENABLE_IMGUI_TEST_ENGINE
#include "CImGuiTestEngine.h"
#include "imgui_te_engine.h"
extern void RegisterDummyAppTests(ImGuiTestEngine *engine);
static bool sRunTests = false;
static int sWarmupFrames = 0;
static bool sTestsDone = false;
#endif

static bool sRunSuiteAll = false;
static bool sRunSuiteTest = false;
static bool sExitAfterTests = false;
static const char *sSuiteTestName = NULL;
static bool sSuiteTestScheduled = false;
static int sSuiteWarmupFrames = 0;

const char *MT_GetMainWindowTitle()
{
	return "DummyApp";
}

const char *MT_GetSettingsFolderName()
{
	return "MTEngineSDLDummyApp";
}

void MT_GetDefaultWindowPositionAndSize(int *defaultWindowPosX, int *defaultWindowPosY, int *defaultWindowWidth, int *defaultWindowHeight, bool *maximized)
{
	// 640x480 is a FIXED PIXEL CONSTANT, so on a HiDPI display it has to grow
	// with everything else -- otherwise the first-run window is half the size
	// the author meant and the scaled UI immediately overflows it. The engine
	// asks for this during VID_Init, before there is a window and before
	// DummyAppResolveUiScale() runs, so the answer comes from the primary
	// display rather than from the resolved setting; that is where a first-run
	// window lands anyway.
	//
	// First run only: once the window has been moved or resized the engine's
	// stored MainWindow* keys replace these.
	const float scale = MT_DetectDisplayUiScale();

	*defaultWindowPosX = SDL_WINDOWPOS_CENTERED;
	*defaultWindowPosY = SDL_WINDOWPOS_CENTERED;
	*defaultWindowWidth = (int)(640 * scale);
	*defaultWindowHeight = (int)(480 * scale);
	*maximized = false;
}

// ---------------------------------------------------------------------------
// The HiDPI UI scale, resolved once at startup.
//
// WHY AN APP HAS TO DO THIS AT ALL. SDL3 declares PER_MONITOR_AWARE_V2 for the
// process, so on Windows and Linux the app is handed REAL PHYSICAL PIXELS and
// one ImGui unit is one of them -- nothing scales the UI unless the app says
// so, and on a 200% display every other window on screen is twice the size of
// this one. macOS is the opposite: SDL reports points and the density rides in
// DisplayFramebufferScale, so the OS already scaled and doing it again would
// double. MT_DetectDisplayUiScale() knows that asymmetry so no app has to;
// see MTEngineSDL/docs/hidpi-ui-scaling.md.
//
// THE POLICY, and it needs no second config key. GetFloat() returns the default
// when the key is ABSENT, so passing the detected scale as that default is
// exactly "auto-detect on first run, honour the user's choice ever after": once
// the GUI Scale menu has written ui.guiScale, this reads it back and the
// display is not consulted.
//
// Headless is 1.0 without asking: MT_DetectDisplayUiScale() returns 1.0 there
// by design, because a suite that asserts on pixel geometry must not move with
// the build machine's monitor.
//
// MT_SetUiScale(), not a raw write to style.FontScaleMain. FontScaleMain scales
// TEXT only; the engine call also scales the GEOMETRY table (padding, widget
// heights) and re-applies the active theme at the new scale instead of scaling
// a style underneath it -- and it is idempotent, which a hand-rolled
// ScaleAllSizes() is not, since that multiplies into the CURRENT sizes and
// squares on a second call.
//
// Called BEFORE the first view is constructed so that every MT_UiScaled()
// constant a view writes in its constructor is already right.
// ---------------------------------------------------------------------------
static void DummyAppResolveUiScale()
{
	// HEADLESS IS PINNED TO 1.0 WHATEVER THE SETTINGS SAY, and reading the
	// SETTING rather than only the display is the part that matters here.
	// MT_DetectDisplayUiScale() already answers 1.0 in headless mode, so the
	// detect path was safe on its own -- but the config read below would then
	// put a stored value straight back on top of it. This machine had
	// ui.guiScale: 1.5 in settings.hjson from an interactive session, and the
	// first headless run after this function was written duly logged 1.50: the
	// suites were asserting pixel geometry at a scale that came from whoever
	// last used the GUI Scale menu. That is a failure which reproduces on one
	// developer's machine and nowhere else, so it does not get to depend on
	// luck.
	if (gHeadlessMode)
	{
		MT_SetUiScale(1.0f);
		LOGM("DummyAppResolveUiScale: 1.00 (headless, pinned)");
		return;
	}

	// AUTO ON FIRST RUN, the user's choice ever after -- and it needs no second
	// "is it auto?" config key. GetFloat() falls back to its default only when
	// the key is ABSENT, so passing the detected scale AS that default says
	// exactly that: no ui.guiScale yet means follow the display; once the GUI
	// Scale menu has written one, the display is not consulted again.
	float scale = MT_DetectDisplayUiScale();
	if (gApplicationDefaultConfig != NULL)
		gApplicationDefaultConfig->GetFloat("ui.guiScale", &scale, scale);

	MT_SetUiScale(scale);
	LOGM("DummyAppResolveUiScale: %.2f", MT_GetUiScale());
}

void MT_PreInit()
{
}

void MT_GuiPreInit()
{
}

void MT_PostInit()
{
	LOGM("MT_PostInit");

	// Parse test CLI flags
	for (int i = 0; i < (int)sysCommandLineArguments.size(); i++)
	{
		const char *arg = sysCommandLineArguments[i];
		if (strcmp(arg, "--run-test") == 0 && i + 1 < (int)sysCommandLineArguments.size())
		{
			sRunSuiteTest = true;
			sSuiteTestName = sysCommandLineArguments[++i];
		}
		else if (strcmp(arg, "--run-suite") == 0)
		{
			sRunSuiteAll = true;
		}
		else if (strcmp(arg, "--exit-after-tests") == 0)
		{
			sExitAfterTests = true;
		}
#if MT_ENABLE_IMGUI_TEST_ENGINE
		else if (strcmp(arg, "--run-tests") == 0)
		{
			sRunTests = true;
		}
#endif
	}

	// Initialize i18n before creating views so _T() / _TID() are ready
	CDummyAppI18n::Init();

	// BEFORE the view exists: resolves the HiDPI scale and applies it to the
	// ImGui style, so the first frame is drawn at the right size and any
	// MT_UiScaled() constant a view constructor writes is already correct.
	DummyAppResolveUiScale();

	CViewDummyAppMain *viewMain = new CViewDummyAppMain(50, 50, 640 + 50, 480 + 50);

	// Install a Latin-Extended UI font (Polish etc.) + markdown fonts before the
	// first frame builds the ImGui font atlas.
	viewMain->LoadFonts();

	guiMain->SetView(viewMain);

	if (sRunSuiteAll || sRunSuiteTest)
	{
		// Schedule after a few warmup frames so the view is ready
		sSuiteWarmupFrames = 3;
		sSuiteTestScheduled = true;
	}

#if MT_ENABLE_IMGUI_TEST_ENGINE
	if (sRunTests)
	{
		CImGuiTestEngine::Init();
		RegisterDummyAppTests(CImGuiTestEngine::GetEngine());
		sWarmupFrames = 5;
	}
#endif

	LOGM("DummyApp initialized");
}

void MT_Render()
{
	// CTestSuite: run after warmup frames
	if (sSuiteTestScheduled && sSuiteWarmupFrames > 0)
	{
		sSuiteWarmupFrames--;
		if (sSuiteWarmupFrames == 0)
		{
			sSuiteTestScheduled = false;
			if (sRunSuiteAll)
				CDummyAppTestSuite::RunFromCLI(NULL);
			else if (sRunSuiteTest)
				CDummyAppTestSuite::RunFromCLI(sSuiteTestName);

			if (sExitAfterTests)
				SYS_Shutdown();
		}
	}

#if MT_ENABLE_IMGUI_TEST_ENGINE
	if (sRunTests && sWarmupFrames > 0)
	{
		sWarmupFrames--;
		if (sWarmupFrames == 0)
			CImGuiTestEngine::QueueAllTests();
	}

	if (sRunTests && sWarmupFrames == 0 && !sTestsDone)
	{
		if (CImGuiTestEngine::IsTestQueueEmpty())
		{
			sTestsDone = true;
			int tested = 0, success = 0;
			CImGuiTestEngine::GetResultSummary(&tested, &success);
			LOGM("ImGui tests: %d/%d passed", success, tested);
			// Write the same results file the CTestSuite path writes, so
			// tests/run_test.sh and CI parse one format for both suites instead
			// of grepping log text for a number.
			CImGuiTestEngine::WriteResults();
			if (sExitAfterTests)
				SYS_Shutdown();
		}
	}
#endif
}

void MT_PostRenderEndFrame()
{
#if MT_ENABLE_IMGUI_TEST_ENGINE
	if (sRunTests)
		CImGuiTestEngine::PostSwap();
#endif
}

void MT_Shutdown()
{
	LOGD("MT_Shutdown");
}

ImU32 ImPlotColorsExtensionGetterCallback(int index)
{
	return 0;
}
