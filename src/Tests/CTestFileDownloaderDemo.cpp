#include "CTestFileDownloaderDemo.h"
#include "CGuiMain.h"
#include "DBG_Log.h"
#include "CViewDummyAppMain.h"
#include "CViewFileDownloaderDemo.h"
#include "RES_ResourceManager.h"
#include "SYS_Funct.h"
#include <filesystem>

#define ASSERT_TRUE(cond, msg)                                   \
    do {                                                          \
        if (!(cond)) {                                            \
            char buf[256];                                        \
            snprintf(buf, sizeof(buf), "FAIL: %s", msg);         \
            LOGD("CTestFileDownloaderDemo: %s", buf);            \
            TestCompleted(false, buf);                            \
            return;                                               \
        }                                                         \
        StepCompleted(stepNum++, true, msg);                      \
    } while (0)

CTestFileDownloaderDemo::CTestFileDownloaderDemo() {}
CTestFileDownloaderDemo::~CTestFileDownloaderDemo() {}

void CTestFileDownloaderDemo::Run(ITestCallback *callback)
{
    this->callback = callback;
    isRunning = true;
    int stepNum = 1;

    CViewDummyAppMain *viewMain = dynamic_cast<CViewDummyAppMain *>(guiMain->currentView);
    ASSERT_TRUE(viewMain != nullptr, "main view is CViewDummyAppMain");
    ASSERT_TRUE(viewMain->viewFileDownloaderDemo != nullptr, "file downloader example view is created");

    CViewFileDownloaderDemo *view = viewMain->viewFileDownloaderDemo;

    // Exercise the exact code path the "Start Download" button calls.
    view->StartDemoDownload();

    bool finished = false;
    for (int elapsedMs = 0; elapsedMs < 10000 && isRunning; elapsedMs += 50)
    {
        if (view->downloader.Poll())
        {
            finished = true;
            break;
        }
        SYS_Sleep(50);
    }

    ASSERT_TRUE(finished, "download finishes within the timeout");
    ASSERT_TRUE(view->downloader.IsSuccess(), view->downloader.GetLastError().c_str());
    ASSERT_TRUE(std::filesystem::exists(view->demoDstPath), "the downloaded file exists on disk");

    std::string sourceDir = RES_ResolveResourceDir("assets/fonts/", "Roboto-Medium.ttf");
    std::string sourcePath = sourceDir + "Roboto-Medium.ttf";
    std::error_code ec;
    uintmax_t sourceSize = std::filesystem::file_size(sourcePath, ec);
    uintmax_t downloadedSize = std::filesystem::file_size(view->demoDstPath, ec);
    ASSERT_TRUE(!ec && sourceSize == downloadedSize, "downloaded file size matches the source asset");

    std::filesystem::remove(view->demoDstPath, ec);

    LOGD("CTestFileDownloaderDemo: all steps passed");
    TestCompleted(true, "File downloader example completed a real local download");
}

void CTestFileDownloaderDemo::Cancel()
{
    isRunning = false;
}
