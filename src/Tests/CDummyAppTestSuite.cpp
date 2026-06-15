#include "CDummyAppTestSuite.h"
#include "CTestAppStartup.h"

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
	// Add new tests here as the project grows
}

void CDummyAppTestSuite::RunFromCLI(const char *testName)
{
	CDummyAppTestSuite *suite = new CDummyAppTestSuite();
	CTestSuite::RunFromCLI(suite, testName);
}
