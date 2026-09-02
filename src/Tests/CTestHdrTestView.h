#pragma once

#include "CTest.h"

// Headless test for the HDR test bench (CViewHdrTest).
//
// WHAT CAN AND CANNOT BE TESTED HERE. Whether an above-white patch LOOKS
// brighter than white is a question about a panel and a pair of eyes, and no
// automated test can answer it. What IS testable is everything the view does
// before the photons: that the pattern carries the linear values it claims, that
// the surface encode is the engine's one copy of the sRGB curve and not a second
// one, that the peak statistic matches the pattern, and that the resident-format
// funnel's answer agrees with what the running backend can actually upload.
//
// The last of those is the one that would otherwise rot silently: an engine
// change that stopped tone-mapping float images on a non-float backend would
// produce a garbled texture, not a crash, and nothing else in this repo looks.
//
// Run: ./MTEngineSDLDummyApp --headless --log-dir /tmp --run-test HdrTestView --exit-after-tests
class CTestHdrTestView : public CTest
{
public:
	CTestHdrTestView();
	virtual ~CTestHdrTestView();

	virtual const char *GetName() override { return "HdrTestView"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
