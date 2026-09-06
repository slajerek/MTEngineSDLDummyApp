#include "CTestProjectPath.h"
#include "SYS_FileSystem.h"
#include "DBG_Log.h"

#include <string>

#define ASSERT_TRUE(cond, msg)                                   \
    do {                                                          \
        if (!(cond)) {                                            \
            char buf[512];                                        \
            snprintf(buf, sizeof(buf), "FAIL: %s", msg);         \
            LOGD("CTestProjectPath: %s", buf);                   \
            TestCompleted(false, buf);                            \
            return;                                               \
        }                                                         \
        StepCompleted(stepNum++, true, msg);                      \
    } while (0)

CTestProjectPath::CTestProjectPath() {}
CTestProjectPath::~CTestProjectPath() {}

void CTestProjectPath::Run(ITestCallback *callback)
{
	this->callback = callback;
	isRunning = true;
	int stepNum = 1;

	const std::string &root = CTest::ProjectRootPath();
	LOGD("CTestProjectPath: project root = '%s'", root.c_str());
	ASSERT_TRUE(!root.empty(), "a project root is found above the start directory");

	// Two files that exist ONLY at the root. From the root this is the
	// identity; from prod/ it is a four-level walk. Both must pass.
	std::string caps = CTest::ResolveProjectPath("mtengine.caps");
	ASSERT_TRUE(!caps.empty() && SYS_FileExists(caps.c_str()),
	            "mtengine.caps resolves under the root");

	std::string runner = CTest::ResolveProjectPath("tests/run_test.sh");
	ASSERT_TRUE(!runner.empty() && SYS_FileExists(runner.c_str()),
	            "a tests/ path resolves under the root -- from the root AND from prod/");

	// The contract's edges: an empty relative path is the root itself, and a
	// NULL one is refused rather than dereferenced.
	ASSERT_TRUE(CTest::ResolveProjectPath("") == root, "an empty relative path is the root");
	ASSERT_TRUE(CTest::ResolveProjectPath(NULL).empty(), "NULL is refused, not dereferenced");

	TestCompleted(true, "fixtures resolve under the project root from wherever the binary runs");
}

void CTestProjectPath::Cancel()
{
	isRunning = false;
}
