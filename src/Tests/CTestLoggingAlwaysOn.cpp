#include "CTestLoggingAlwaysOn.h"
#include "DBG_Log.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <chrono>

#define ASSERT_TRUE(cond, msg)                                   \
    do {                                                          \
        if (!(cond)) {                                            \
            char buf[512];                                        \
            snprintf(buf, sizeof(buf), "FAIL: %s", msg);         \
            TestCompleted(false, buf);                            \
            return;                                               \
        }                                                         \
        StepCompleted(stepNum++, true, msg);                      \
    } while (0)

CTestLoggingAlwaysOn::CTestLoggingAlwaysOn() {}
CTestLoggingAlwaysOn::~CTestLoggingAlwaysOn() {}

static unsigned long NowMillis()
{
	using namespace std::chrono;
	return (unsigned long)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static bool FileContains(const char *path, const char *needle)
{
	FILE *f = fopen(path, "rb");
	if (f == NULL)
		return false;
	std::string text;
	char buf[8192];
	size_t got;
	while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
		text.append(buf, got);
	fclose(f);
	return text.find(needle) != std::string::npos;
}

void CTestLoggingAlwaysOn::Run(ITestCallback *callback)
{
	this->callback = callback;
	isRunning = true;
	int stepNum = 1;

	const char *logPath = LOG_GetLogFilePath();
	ASSERT_TRUE(logPath != NULL && logPath[0] != '\0',
	            "LOG_Init opened a log file (every platform has a file sink now)");

	// A token no other line can contain, so a match is this run's write.
	char token[64];
	snprintf(token, sizeof(token), "MT_LOGCHECK_ERROR_%lu", NowMillis());
	LOGError("%s", token);
	ASSERT_TRUE(FileContains(logPath, token),
	            "LOGError reached the log file -- the always-on path, whatever MT_DEBUG_LOGS says");

	char verbose[64];
	snprintf(verbose, sizeof(verbose), "MT_LOGCHECK_VERBOSE_%lu", NowMillis());
	LOGD("%s", verbose);
#if MT_DEBUG_LOGS
	ASSERT_TRUE(FileContains(logPath, verbose),
	            "MT_DEBUG_LOGS=1: LOGD reached the log file");
#else
	ASSERT_TRUE(!FileContains(logPath, verbose),
	            "MT_DEBUG_LOGS=0: LOGD compiled to nothing -- the gate is real, not a level default");
#endif

	TestCompleted(true, MT_DEBUG_LOGS ? "logs on: errors and debug both written"
	                                  : "logs off: errors written, debug compiled out");
}

void CTestLoggingAlwaysOn::Cancel()
{
	isRunning = false;
}
