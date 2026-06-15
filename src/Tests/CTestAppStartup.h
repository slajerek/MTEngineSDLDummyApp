#pragma once

#include "CTest.h"

// Headless smoke test: verifies the app init lifecycle completed.
// Pattern: CTest-based async integration test.
// Run: ./MTEngineSDLDummyApp --headless --run-test AppStartup --exit-after-tests
class CTestAppStartup : public CTest
{
public:
	CTestAppStartup();
	virtual ~CTestAppStartup();

	virtual const char *GetName() override { return "AppStartup"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
