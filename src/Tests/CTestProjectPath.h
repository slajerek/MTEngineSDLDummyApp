#pragma once

#include "CTest.h"

// CTestProjectPath
//
// The engine's CTest::ResolveProjectPath() is what every fixture path in every
// app goes through, so this is the test that proves it works from BOTH places
// a binary runs -- the git root (development build) and
// platform/<P>/prod/<arch>/ (final build, `tests/run_test.sh --package`).
// It asks for two files that exist only at the root, so a resolver that
// returned the cwd unchanged would pass from the root and fail from prod/,
// which is exactly the case it exists for.
class CTestProjectPath : public CTest
{
public:
	CTestProjectPath();
	virtual ~CTestProjectPath();

	virtual const char *GetName() override { return "ProjectPath"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
