#ifndef _CViewHdrTest_h_
#define _CViewHdrTest_h_

#include "CGuiView.h"
#include "SYS_Defs.h"
#include "ERenderTextureFormat.h"
#include <vector>

class CSlrImage;

// ===========================================================================
// The HDR test bench
// ===========================================================================
//
// WHAT THIS IS FOR. HDR is the one engine feature that cannot be verified by
// reading a number: the whole question is whether a value ABOVE SDR white
// arrives at the panel brighter than white, and the only instrument for that is
// a human looking at a pattern. This view generates those patterns, states the
// pipeline's live capability in one line, and prints every number a developer
// would otherwise have to dig out of four different headers.
//
// WHY A TEXTURE AND NOT ImDrawList. ImGui's vertex colours are packed ImU32 --
// eight bits per channel -- so NOTHING drawn through ImGui's own primitives can
// exceed 1.0. Above-white content can only reach the surface as an RGBA16F
// TEXTURE, which is why every pattern here is generated into a CImageData of
// type IMG_TYPE_RGBA_16F and uploaded, rather than drawn with rectangles. A
// version of this view built on ImDrawList would look plausible, produce a
// clipped image on every machine, and prove nothing.
//
// THE ONE PIECE OF ARITHMETIC THAT MATTERS. Patterns are authored in LINEAR
// with 1.0 == SDR reference white, because that is the only unit in which "two
// stops above white" means anything. The Metal surface is extended-sRGB
// ENCODED, not linear (CRenderBackendMetal does not override
// GetSurfaceIsLinearColorSpace, so it answers false), and RGBA16F has no
// hardware sRGB-decode variant -- see ERenderTextureFormat.h -- so whatever
// texels carry is what the surface receives. The pattern is therefore run
// through SrgbExtendedEncode() before it becomes half floats, and
// CImageData::floatIsSurfaceEncoded is set to say so. Skipping that step is
// exactly the "washed out" class of bug the engine's own S-4 notes describe:
// it would display mid-grey at 0.21 and make every above-white patch too dim.
//
// The "Surface encoding" control can turn the encode off deliberately, because
// SEEING the wrong answer beside the right one is the fastest way to recognise
// it in a real app.
//
// GRACEFUL DEGRADATION IS PART OF THE TEST. On a backend with no float texture
// support (OpenGL) the engine's resident-format funnel tone-maps the pattern to
// 8 bits at upload -- CSlrImage::ApplyResidentFormat. This view does not work
// around that; it reports which of the two answers the funnel gave, so the SDR
// fallback is observable rather than merely assumed. That is why the resident
// format is read back from the CSlrImage after upload instead of being
// predicted from the backend.
// ===========================================================================

enum EHdrTestPattern
{
	// Patches at 1x, 2x, 4x ... SDR white. The canonical "how many stops above
	// white can this display actually show" instrument.
	HDR_PATTERN_STOP_LADDER = 0,

	// Exactly 1.0 above, the requested peak below. The single most decisive
	// test in the set: if the two halves look identical, above-white content is
	// not reaching the panel, whatever the capability readout claims.
	HDR_PATTERN_SDR_HDR_SPLIT,

	// A smooth 0 -> peak sweep. Shows banding, and shows WHERE the pipeline
	// stops responding (the ramp goes flat at the clipping point).
	HDR_PATTERN_LINEAR_RAMP,

	// A radial falloff peaking in the centre -- a synthetic specular highlight.
	// Reveals the SHAPE of any tone-map roll-off, which flat patches cannot.
	HDR_PATTERN_HIGHLIGHT_DISC,

	// Eleven steps at and below white, then the above-white steps. Exists to
	// catch the regression that matters most: an HDR path that brightens
	// highlights correctly while quietly breaking the ordinary SDR ramp.
	HDR_PATTERN_GREY_STEPS,

	// R/G/B/C/M/Y/W columns crossed with stop rows. Range and gamut together,
	// because a wide-gamut primary pushed above white is where the two interact.
	HDR_PATTERN_COLOR_VOLUME,

	// Alternating white and peak squares. Small bright areas next to dark ones
	// is what provokes local-dimming halos and panel-level power limiting.
	HDR_PATTERN_CHECKERBOARD,

	HDR_PATTERN_COUNT
};

class CViewHdrTest : public CGuiView
{
public:
	CViewHdrTest(const char *name, float posX, float posY, float posZ,
				 float sizeX, float sizeY);
	virtual ~CViewHdrTest();

	virtual void RenderImGui() override;

	// -------------------------------------------------------------------
	// Test seam
	//
	// The suite drives generation directly rather than through the UI: a
	// CTestSuite test runs headless with no mouse, and asserting on the
	// generated pixels is the only way to check the pattern arithmetic
	// without a display to photograph.
	// -------------------------------------------------------------------

	// Generate the pattern and upload it. Safe to call before the first
	// frame; a NULL render backend is a no-op rather than a crash.
	void RegenerateNow();

	bool IsPatternUploaded() const;

	// The pattern's peak LINEAR value, 1.0 == SDR reference white.
	float GetPeakLinear() const { return statsPeakLinear; }

	// What the resident-format funnel actually chose. RGBA16F means above-white
	// values survived to the GPU; RGBA8 means they were tone-mapped away.
	ERenderTextureFormat GetResidentFormat() const;

	// Linear RGBA at a pattern pixel, straight out of the authoring buffer --
	// never read back from the texture, so it is the same number on every
	// backend. Returns false outside the image.
	bool GetPatternPixelLinear(int x, int y, float *r, float *g, float *b) const;

	// Pattern selection, for a test that wants to sweep all of them.
	void SetPattern(EHdrTestPattern p);
	EHdrTestPattern GetPattern() const { return pattern; }

	// The parameters a test needs in order to compute its own expected values
	// rather than repeating the generator's geometry. A test that hardcodes
	// "the last bar starts at 7*W/8" stops testing the generator and starts
	// testing its own copy of it.
	void GetPatternSize(int *outW, int *outH) const { *outW = patternWidth; *outH = patternHeight; }
	float GetPeakStops() const { return peakStops; }
	void SetPeakStops(float stops) { peakStops = stops; needsRegenerate = true; }

private:
	// ---- generation -------------------------------------------------
	void GeneratePatternLinear();
	void UploadPattern();
	void ComputeStats();
	void ReleaseImage();

	// ---- UI sections ------------------------------------------------
	void RenderStatusBanner();
	void RenderControlsColumn();
	void RenderPatternTab();
	void RenderDisplayTab();
	void RenderStatsTab();
	void RenderPatternImageAndProbe();
	void RenderLevelLegend();

	// ---- live capability --------------------------------------------
	// Every one of these is a POLL. macOS grants headroom lazily and then keeps
	// moving it with display brightness and ambient light, so a value cached at
	// init would be wrong within seconds of the window appearing.
	float LiveHeadroom() const;
	bool  SurfaceIsExtendedRange() const;
	bool  SurfaceIsLinear() const;
	bool  BackendSupportsFloatTextures() const;

	// ---- pattern parameters -----------------------------------------
	EHdrTestPattern pattern = HDR_PATTERN_STOP_LADDER;

	int   patternWidth  = 768;
	int   patternHeight = 384;

	// Peak in STOPS above SDR white; the linear peak is 2^peakStops. Stops
	// rather than a linear multiplier because that is the unit displays are
	// specified in, and because a linear slider spends nine tenths of its
	// travel in a range no panel can show.
	float peakStops = 3.0f;          // 8x SDR white

	int   stepCount = 8;
	float baseLevel = 0.18f;         // 18% grey, the photographic mid-tone
	float tintR = 1.0f, tintG = 1.0f, tintB = 1.0f;

	// Run the pattern through SrgbExtendedEncode before it becomes half
	// floats. ON is correct for this engine's surfaces -- see the header
	// comment. OFF is a deliberate demonstration of the wrong answer.
	bool  encodeForSurface = true;

	// Assumed nits for SDR white, used ONLY for the estimated-nits column.
	// 203 is BT.2408 reference white, the same anchor
	// VideoTransfer::kSdrReferenceWhiteNits uses. It is an ASSUMPTION and
	// labelled as one: how many nits the display actually gives our 1.0 is the
	// OS's decision, made with display brightness on macOS and the SDR slider
	// on Windows.
	float assumedSdrWhiteNits = 203.0f;

	bool  needsRegenerate = true;

	// ---- generated data ---------------------------------------------
	// The authoring buffer, LINEAR, RGB interleaved (no alpha: every pattern
	// here is opaque, and carrying a channel that is always 1.0 would just make
	// the probe arithmetic wrong-by-one more often).
	std::vector<float> linearRGB;

	CSlrImage *patternImage = NULL;
	ERenderTextureFormat uploadedFormat = RENDER_TEXTURE_RGBA8;
	bool patternUploaded = false;
	size_t uploadedBytes = 0;

	// ---- statistics --------------------------------------------------
	float statsPeakLinear = 0.0f;
	float statsMinLinear  = 0.0f;
	float statsMeanLinear = 0.0f;
	float statsFractionAboveWhite = 0.0f;

	// Histogram over log2(linear), one bin per third of a stop from -10 to +7.
	static const int kHistogramBins = 51;
	static const int kHistogramMinStop = -10;
	static const int kHistogramMaxStop = 7;
	float histogramCounts[kHistogramBins];
	float histogramStops[kHistogramBins];

	// ---- headroom trace ----------------------------------------------
	// A ring buffer of the live headroom poll. This exists because macOS does
	// NOT grant headroom at once: it reads 1.0 for the first seconds after the
	// request and then ramps. Anybody who samples it once at startup concludes
	// HDR is unavailable, and a plot is the shortest way to make that ramp
	// impossible to misread.
	static const int kTraceLength = 600;
	float traceHeadroom[kTraceLength];
	float traceTime[kTraceLength];
	int   traceCount = 0;
	int   traceHead = 0;
	double traceElapsed = 0.0;

	// ---- EDR metadata ------------------------------------------------
	float edrMetadataPeak = 4.0f;

	// ---- probe -------------------------------------------------------
	bool  probeValid = false;
	int   probeX = 0, probeY = 0;
	float probeR = 0.0f, probeG = 0.0f, probeB = 0.0f;
};

#endif
