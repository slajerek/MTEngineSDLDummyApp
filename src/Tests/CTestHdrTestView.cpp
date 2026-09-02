#include "CTestHdrTestView.h"
#include "CGuiMain.h"
#include "DBG_Log.h"
#include "CViewDummyAppMain.h"
#include "CViewHdrTest.h"
#include "VID_Main.h"
#include "CRenderBackend.h"
#include "MT_SrgbCurve.h"
#include <cmath>
#include <cstdio>

#define ASSERT_TRUE(cond, msg)                                    \
    do {                                                          \
        if (!(cond)) {                                            \
            char buf[256];                                        \
            snprintf(buf, sizeof(buf), "FAIL: %s", msg);          \
            LOGD("CTestHdrTestView: %s", buf);                    \
            TestCompleted(false, buf);                            \
            return;                                               \
        }                                                         \
        StepCompleted(stepNum++, true, msg);                      \
    } while (0)

// Half floats carry about three decimal digits, and the view's authoring buffer
// is plain float -- but the values asserted below are all exact powers of two or
// zero, which both formats represent exactly. The tolerance is here for the
// arithmetic that produces them (powf), not for storage.
static const float kEps = 1e-4f;

static bool NearlyEqual(float a, float b)
{
	const float d = a - b;
	return (d < 0.0f ? -d : d) <= kEps * (1.0f + (b < 0.0f ? -b : b));
}

CTestHdrTestView::CTestHdrTestView() {}
CTestHdrTestView::~CTestHdrTestView() {}

void CTestHdrTestView::Run(ITestCallback *callback)
{
	this->callback = callback;
	isRunning = true;
	int stepNum = 1;

	LOGD("CTestHdrTestView: running");

	ASSERT_TRUE(guiMain != nullptr, "guiMain is initialized");
	CViewDummyAppMain *viewMain = dynamic_cast<CViewDummyAppMain *>(guiMain->currentView);
	ASSERT_TRUE(viewMain != nullptr, "main view is CViewDummyAppMain");
	ASSERT_TRUE(viewMain->viewHdrTest != nullptr, "HDR test example view is created");

	CViewHdrTest *hdr = viewMain->viewHdrTest;

	viewMain->OpenExampleHdrTest();
	ASSERT_TRUE(hdr->visible, "HDR test example opens its view");

	int W = 0, H = 0;
	hdr->GetPatternSize(&W, &H);
	ASSERT_TRUE(W > 8 && H > 8, "pattern has a usable size");

	// ------------------------------------------------------------------
	// The transfer curve the whole view depends on.
	//
	// Asserted against the engine's ONE copy (MT_SrgbCurve.h) rather than
	// against decimals typed here: a test with its own constants would keep
	// passing after the engine's curve changed, which is the opposite of what
	// it is for. What IS pinned by value is the two properties that make the
	// curve usable for HDR at all.
	// ------------------------------------------------------------------
	ASSERT_TRUE(NearlyEqual(SrgbExtendedEncode(1.0f), 1.0f),
				"SDR white encodes to exactly 1.0 -- white stays white");
	ASSERT_TRUE(SrgbExtendedEncode(4.0f) > 1.0f,
				"the curve CONTINUES above 1.0 rather than clamping there");
	ASSERT_TRUE(NearlyEqual(SrgbExtendedDecode(SrgbExtendedEncode(6.5f)), 6.5f),
				"encode/decode round-trips an above-white value");

	// ------------------------------------------------------------------
	// Pattern arithmetic. Every assertion below is on a point that is fixed by
	// the pattern's DEFINITION, not by its layout arithmetic -- a corner, an
	// edge column -- so this test does not become a second copy of the
	// generator it is checking.
	// ------------------------------------------------------------------
	hdr->SetPeakStops(3.0f);                    // peak = 8x SDR white
	const float expectedPeak = powf(2.0f, 3.0f);

	float r = 0.0f, g = 0.0f, b = 0.0f;

	// --- SDR / HDR split: top is white by definition, bottom is the peak ---
	hdr->SetPattern(HDR_PATTERN_SDR_HDR_SPLIT);
	hdr->RegenerateNow();

	ASSERT_TRUE(hdr->GetPatternPixelLinear(1, 1, &r, &g, &b), "split: top corner is inside the pattern");
	ASSERT_TRUE(NearlyEqual(r, 1.0f) && NearlyEqual(g, 1.0f) && NearlyEqual(b, 1.0f),
				"split: the top half is exactly SDR white");

	ASSERT_TRUE(hdr->GetPatternPixelLinear(1, H - 2, &r, &g, &b), "split: bottom corner is inside the pattern");
	ASSERT_TRUE(NearlyEqual(r, expectedPeak) && NearlyEqual(g, expectedPeak) && NearlyEqual(b, expectedPeak),
				"split: the bottom half is exactly the requested peak");

	ASSERT_TRUE(NearlyEqual(hdr->GetPeakLinear(), expectedPeak),
				"split: the reported peak statistic matches the pattern");

	// --- Linear ramp: black at one edge, peak at the other ---
	hdr->SetPattern(HDR_PATTERN_LINEAR_RAMP);
	hdr->RegenerateNow();

	ASSERT_TRUE(hdr->GetPatternPixelLinear(0, H / 2, &r, &g, &b), "ramp: left edge is inside the pattern");
	ASSERT_TRUE(NearlyEqual(r, 0.0f), "ramp: starts at zero");

	ASSERT_TRUE(hdr->GetPatternPixelLinear(W - 1, H / 2, &r, &g, &b), "ramp: right edge is inside the pattern");
	ASSERT_TRUE(NearlyEqual(r, expectedPeak), "ramp: ends at exactly the requested peak");

	// --- Stop ladder: the first bar is SDR white, and the surround is not ---
	hdr->SetPattern(HDR_PATTERN_STOP_LADDER);
	hdr->RegenerateNow();

	ASSERT_TRUE(hdr->GetPatternPixelLinear(1, H / 2, &r, &g, &b), "ladder: first bar is inside the pattern");
	ASSERT_TRUE(NearlyEqual(r, 1.0f), "ladder: the first step is exactly SDR white");
	ASSERT_TRUE(NearlyEqual(hdr->GetPeakLinear(), expectedPeak),
				"ladder: the last step reaches exactly the requested peak");

	ASSERT_TRUE(hdr->GetPatternPixelLinear(1, 0, &r, &g, &b), "ladder: surround is inside the pattern");
	ASSERT_TRUE(r < 1.0f, "ladder: the surround sits below white, so patches have a reference");

	// --- Peak follows the control, in both directions ---
	hdr->SetPeakStops(0.0f);
	hdr->RegenerateNow();
	ASSERT_TRUE(NearlyEqual(hdr->GetPeakLinear(), 1.0f),
				"zero stops means an ordinary SDR pattern, peak exactly 1.0");

	hdr->SetPeakStops(3.0f);
	hdr->RegenerateNow();
	ASSERT_TRUE(NearlyEqual(hdr->GetPeakLinear(), expectedPeak), "the peak follows the stops control back up");

	// --- Every pattern generates without a peak that contradicts the control ---
	for (int p = 0; p < HDR_PATTERN_COUNT; p++)
	{
		hdr->SetPattern((EHdrTestPattern)p);
		hdr->RegenerateNow();
		char msg[128];
		snprintf(msg, sizeof(msg), "pattern %d generates with a peak inside the requested range", p);
		ASSERT_TRUE(hdr->GetPeakLinear() > 0.0f && hdr->GetPeakLinear() <= expectedPeak + kEps, msg);
	}

	// ------------------------------------------------------------------
	// The resident-format funnel.
	//
	// BOTH answers are correct; which one is correct HERE depends on the
	// backend. Asserting the agreement rather than one fixed value is what lets
	// this run unchanged on Metal, D3D11 and OpenGL -- and it is the assertion
	// that would catch the funnel silently uploading float pixels to a backend
	// that cannot hold them, which garbles a texture rather than crashing.
	// ------------------------------------------------------------------
	ASSERT_TRUE(gRenderBackend != nullptr, "a render backend exists (headless still creates one)");

	const bool backendFloat = gRenderBackend->SupportsTextureFormat(RENDER_TEXTURE_RGBA16F);
	if (backendFloat)
	{
		ASSERT_TRUE(hdr->GetResidentFormat() == RENDER_TEXTURE_RGBA16F,
					"backend takes float textures, so above-white survives to the GPU");
	}
	else
	{
		ASSERT_TRUE(hdr->GetResidentFormat() == RENDER_TEXTURE_RGBA8,
					"backend takes no float textures, so the funnel tone-mapped at upload");
	}

	// The surface's own answers must be self-consistent: nothing can be a
	// LINEAR surface without also being an extended-range one, and a backend
	// claiming an RGBA16F surface while refusing float textures would leave the
	// view unable to put anything above white on a surface built to carry it.
	if (gRenderBackend->GetSurfaceIsLinearColorSpace())
	{
		ASSERT_TRUE(gRenderBackend->GetSurfaceIsExtendedRange(),
					"a LINEAR surface is necessarily an extended-range one");
	}
	if (gRenderBackend->GetSurfaceFormat() == RENDER_SURFACE_RGBA16F)
	{
		ASSERT_TRUE(backendFloat,
					"a float SURFACE implies the backend can upload float TEXTURES");
	}

	// Headroom is a multiplier over SDR white and 1.0 means "no extra range".
	// Below 1.0 is not a small value, it is a wrong one, and it would make every
	// clipping verdict in the view backwards.
	ASSERT_TRUE(gRenderBackend->GetDisplayHdrHeadroom() >= 1.0f,
				"granted headroom is never below 1.0");
	ASSERT_TRUE(VID_GetMaxPotentialHdrHeadroom() >= 1.0f,
				"max potential headroom is never below 1.0");

	// Leave the view as it was found: the suite runs inside a live app and the
	// next test should not inherit an open window.
	hdr->SetVisible(false);
	hdr->SetPattern(HDR_PATTERN_STOP_LADDER);

	LOGD("CTestHdrTestView: all steps passed");
	TestCompleted(true, "HDR test bench generates and reports correctly");
}

void CTestHdrTestView::Cancel()
{
	isRunning = false;
}
