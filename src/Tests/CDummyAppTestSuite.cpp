#include "CDummyAppTestSuite.h"
#include "CTestAppStartup.h"
#include "CTestI18nDummyApp.h"
#include "CTestFonts.h"
#include "CTestHdrTestView.h"
#include "CTestFileDownloaderDemo.h"

CDummyAppTestSuite::CDummyAppTestSuite()
{
	defaultTestTimeoutSeconds = 30;
	suiteTimeoutSeconds = 120;
}

CDummyAppTestSuite::~CDummyAppTestSuite()
{
}

void CDummyAppTestSuite::RegisterTests()
{
	tests.push_back(std::make_unique<CTestAppStartup>());
	tests.push_back(std::make_unique<CTestI18nDummyApp>());
	tests.push_back(std::make_unique<CTestFonts>());
	tests.push_back(std::make_unique<CTestHdrTestView>());
	tests.push_back(std::make_unique<CTestFileDownloaderDemo>());
	// Add new tests here as the project grows
}

void CDummyAppTestSuite::RunFromCLI(const char *testName)
{
	CDummyAppTestSuite *suite = new CDummyAppTestSuite();
	CTestSuite::RunFromCLI(suite, testName);
}
