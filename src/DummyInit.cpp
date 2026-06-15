#include "DummyInit.h"
#include "DBG_Log.h"
#include "CGuiMain.h"
#include "MT_API.h"
#include "SYS_CommandLine.h"
#include "CViewDummyAppMain.h"
#include "CDummyAppTestSuite.h"
#include "CTestRunner.h"

#ifdef ENABLE_IMGUI_TEST_ENGINE
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
	*defaultWindowPosX = SDL_WINDOWPOS_CENTERED;
	*defaultWindowPosY = SDL_WINDOWPOS_CENTERED;
	*defaultWindowWidth = 640;
	*defaultWindowHeight = 480;
	*maximized = false;
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
#ifdef ENABLE_IMGUI_TEST_ENGINE
		else if (strcmp(arg, "--run-tests") == 0)
		{
			sRunTests = true;
		}
#endif
	}

	CViewDummyAppMain *viewMain = new CViewDummyAppMain(50, 50, 640 + 50, 480 + 50);
	guiMain->SetView(viewMain);

	if (sRunSuiteAll || sRunSuiteTest)
	{
		// Schedule after a few warmup frames so the view is ready
		sSuiteWarmupFrames = 3;
		sSuiteTestScheduled = true;
	}

#ifdef ENABLE_IMGUI_TEST_ENGINE
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

#ifdef ENABLE_IMGUI_TEST_ENGINE
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
			if (sExitAfterTests)
				SYS_Shutdown();
		}
	}
#endif
}

void MT_PostRenderEndFrame()
{
#ifdef ENABLE_IMGUI_TEST_ENGINE
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
