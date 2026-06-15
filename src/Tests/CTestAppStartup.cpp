#include "CTestAppStartup.h"
#include "CGuiMain.h"
#include "DBG_Log.h"

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

    LOGD("CTestAppStartup: all steps passed");
    TestCompleted(true, "App initialized correctly");
}

void CTestAppStartup::Cancel()
{
    isRunning = false;
}
