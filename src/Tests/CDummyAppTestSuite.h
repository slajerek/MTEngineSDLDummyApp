#pragma once

#include "CTestSuite.h"

class CDummyAppTestSuite : public CTestSuite
{
public:
	CDummyAppTestSuite();
	virtual ~CDummyAppTestSuite();

	virtual void RegisterTests() override;

	// Convenience: creates suite and calls CTestSuite::RunFromCLI
	static void RunFromCLI(const char *testName);
};
