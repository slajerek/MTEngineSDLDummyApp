#pragma once

#include "CTest.h"

// CTestLoggingAlwaysOn
//
// LOGError reaches the log file in EVERY build -- with MT_DEBUG_LOGS=1 and,
// the case that matters, with MT_DEBUG_LOGS=0. Until 2026-09-05 a build with
// logging off compiled LOGError to nothing, including the one inside
// SYS_FatalExit, which is how a Linux CI segfault came with no diagnostic at
// all. This test writes a unique token through LOGError and reads the log file
// LOG_Init opened (LOG_GetLogFilePath) to find it.
//
// The negative half is compile-time: with logs off, a LOGD of a second token
// must NOT appear -- proving the gate is real and not merely a level-mask
// default.
class CTestLoggingAlwaysOn : public CTest
{
public:
	CTestLoggingAlwaysOn();
	virtual ~CTestLoggingAlwaysOn();

	virtual const char *GetName() override { return "LoggingAlwaysOn"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
