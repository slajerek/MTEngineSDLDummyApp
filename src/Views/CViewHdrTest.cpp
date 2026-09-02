#include "CViewHdrTest.h"
#include "CGuiMain.h"
#include "VID_Main.h"
#include "VID_ImageBinding.h"
#include "CRenderBackend.h"
#include "CImageData.h"
#include "CSlrImage.h"
#include "MT_SrgbCurve.h"
#include "DBG_Log.h"
#include "implot.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <string>

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

namespace
{
	// The pattern is authored in LINEAR and the surface wants extended-sRGB
	// ENCODED values -- see the class comment for why that is not optional.
	// One function, one place, so the "off" mode cannot accidentally differ
	// from the "on" mode by anything except the encode itself.
	inline float ToSurface(float linear, bool encode)
	{
		return encode ? SrgbExtendedEncode(linear) : linear;
	}

	// log2 that answers a very negative number for 0 instead of -inf, so it can
	// go straight into a histogram bin index without a special case at every
	// call site.
	inline float SafeLog2(float v)
	{
		return (v > 1e-7f) ? log2f(v) : -24.0f;
	}

	const char *kPatternNames[HDR_PATTERN_COUNT] =
	{
		"Stop ladder",
		"SDR / HDR split",
		"Linear ramp",
		"Highlight disc",
		"Grey steps",
		"Colour volume",
		"Checkerboard",
	};

	const char *kPatternHelp[HDR_PATTERN_COUNT] =
	{
		"Patches at 1x, 2x, 4x ... SDR white. Count the last patch you can still\n"
		"tell apart from its neighbour: that is the display's usable headroom.",

		"Top half is exactly SDR white, bottom half is the requested peak, and\n"
		"each half carries a small square of the other value.\n"
		"If the two halves look the same, above-white content is NOT reaching\n"
		"the panel -- whatever the capability readout above claims.",

		"A smooth 0 -> peak sweep. Banding shows up as visible steps; the point\n"
		"where the ramp stops getting brighter is where the pipeline clips.",

		"A synthetic specular highlight. Flat patches cannot show the SHAPE of a\n"
		"tone-map roll-off; a gradient can.",

		"An ordinary SDR grey ramp on top, the above-white steps below.\n"
		"Here to catch the regression that matters most: an HDR path that gets\n"
		"highlights right while quietly breaking the plain SDR ramp.",

		"Primaries and secondaries crossed with stop rows. Range and gamut\n"
		"interact -- a saturated primary pushed above white is where you see it.",

		"Small bright squares against white. This is what provokes local-dimming\n"
		"halos and panel power limiting; a full-screen flat patch never will.",
	};

	// Colour volume columns: R G B C M Y W.
	const float kVolumeColumns[7][3] =
	{
		{1,0,0}, {0,1,0}, {0,0,1}, {0,1,1}, {1,0,1}, {1,1,0}, {1,1,1},
	};
	const char *kVolumeColumnNames[7] = { "R", "G", "B", "C", "M", "Y", "W" };
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CViewHdrTest::CViewHdrTest(const char *name, float posX, float posY, float posZ,
						   float sizeX, float sizeY)
: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	imGuiNoWindowPadding = false;
	imGuiNoScrollbar = false;

	memset(histogramCounts, 0, sizeof(histogramCounts));
	memset(traceHeadroom, 0, sizeof(traceHeadroom));
	memset(traceTime, 0, sizeof(traceTime));

	for (int i = 0; i < kHistogramBins; i++)
	{
		histogramStops[i] = (float)kHistogramMinStop +
			(float)i * ((float)(kHistogramMaxStop - kHistogramMinStop) / (float)(kHistogramBins - 1));
	}

	// Deliberately NOT generating here. The view is constructed in
	// MT_PostInit() and a first upload would cost a megabyte of texture for a
	// window the user may never open; the first visible frame does it instead.
}

CViewHdrTest::~CViewHdrTest()
{
	ReleaseImage();
}

void CViewHdrTest::ReleaseImage()
{
	if (patternImage != NULL)
	{
		// DESTROY, not DEALLOC. Every regeneration makes a new CSlrImage, so
		// freeing only the pixel buffer would leak the object and its texture
		// name once per parameter change -- and a slider produces a great many
		// parameter changes.
		VID_PostImageDestroy(patternImage);
		patternImage = NULL;
	}
	patternUploaded = false;
	uploadedBytes = 0;
}

// ---------------------------------------------------------------------------
// Live capability polls
// ---------------------------------------------------------------------------

float CViewHdrTest::LiveHeadroom() const
{
	return (gRenderBackend != NULL) ? gRenderBackend->GetDisplayHdrHeadroom() : 1.0f;
}

bool CViewHdrTest::SurfaceIsExtendedRange() const
{
	return (gRenderBackend != NULL) && gRenderBackend->GetSurfaceIsExtendedRange();
}

bool CViewHdrTest::SurfaceIsLinear() const
{
	return (gRenderBackend != NULL) && gRenderBackend->GetSurfaceIsLinearColorSpace();
}

bool CViewHdrTest::BackendSupportsFloatTextures() const
{
	return (gRenderBackend != NULL) &&
		gRenderBackend->SupportsTextureFormat(RENDER_TEXTURE_RGBA16F);
}

ERenderTextureFormat CViewHdrTest::GetResidentFormat() const
{
	return uploadedFormat;
}

bool CViewHdrTest::IsPatternUploaded() const
{
	return patternUploaded;
}

void CViewHdrTest::SetPattern(EHdrTestPattern p)
{
	if (p < 0 || p >= HDR_PATTERN_COUNT)
		return;
	pattern = p;
	needsRegenerate = true;
}

bool CViewHdrTest::GetPatternPixelLinear(int x, int y, float *r, float *g, float *b) const
{
	if (x < 0 || y < 0 || x >= patternWidth || y >= patternHeight)
		return false;
	const size_t off = ((size_t)y * (size_t)patternWidth + (size_t)x) * 3;
	if (off + 2 >= linearRGB.size())
		return false;
	*r = linearRGB[off + 0];
	*g = linearRGB[off + 1];
	*b = linearRGB[off + 2];
	return true;
}

// ---------------------------------------------------------------------------
// Pattern generation -- LINEAR, 1.0 == SDR reference white
// ---------------------------------------------------------------------------

void CViewHdrTest::GeneratePatternLinear()
{
	const int W = patternWidth;
	const int H = patternHeight;
	linearRGB.assign((size_t)W * (size_t)H * 3, 0.0f);

	const float peak = powf(2.0f, peakStops);
	const int steps = (stepCount < 2) ? 2 : stepCount;

	// Value of step i, geometric from SDR white to the requested peak. Written
	// once because five of the seven patterns share it and a second copy would
	// drift.
	auto stepValue = [&](int i) -> float
	{
		return powf(2.0f, peakStops * (float)i / (float)(steps - 1));
	};

	auto put = [&](int x, int y, float r, float g, float b)
	{
		if (x < 0 || y < 0 || x >= W || y >= H)
			return;
		const size_t off = ((size_t)y * (size_t)W + (size_t)x) * 3;
		linearRGB[off + 0] = r;
		linearRGB[off + 1] = g;
		linearRGB[off + 2] = b;
	};

	auto fillRect = [&](int x0, int y0, int x1, int y1, float v,
						float cr, float cg, float cb)
	{
		for (int y = y0; y < y1; y++)
			for (int x = x0; x < x1; x++)
				put(x, y, v * cr, v * cg, v * cb);
	};

	switch (pattern)
	{
		case HDR_PATTERN_STOP_LADDER:
		{
			// A mid-grey surround, so every patch is judged against the same
			// reference rather than against the window background -- which is
			// SDR, drawn by ImGui, and therefore not comparable.
			fillRect(0, 0, W, H, baseLevel, 1.0f, 1.0f, 1.0f);
			const int barTop = H / 8;
			const int barBottom = H - H / 8;
			for (int i = 0; i < steps; i++)
			{
				const int x0 = (int)((float)W * (float)i / (float)steps);
				const int x1 = (int)((float)W * (float)(i + 1) / (float)steps);
				fillRect(x0, barTop, x1, barBottom, stepValue(i), tintR, tintG, tintB);
			}
			break;
		}

		case HDR_PATTERN_SDR_HDR_SPLIT:
		{
			const int mid = H / 2;
			fillRect(0, 0, W, mid, 1.0f, tintR, tintG, tintB);
			fillRect(0, mid, W, H, peak, tintR, tintG, tintB);

			// The inset squares. Simultaneous contrast is far more sensitive
			// than remembering what the other half looked like.
			const int s = H / 6;
			fillRect(W / 2 - s / 2, mid / 2 - s / 2, W / 2 + s / 2, mid / 2 + s / 2,
					 peak, tintR, tintG, tintB);
			fillRect(W / 2 - s / 2, mid + mid / 2 - s / 2, W / 2 + s / 2, mid + mid / 2 + s / 2,
					 1.0f, tintR, tintG, tintB);
			break;
		}

		case HDR_PATTERN_LINEAR_RAMP:
		{
			for (int x = 0; x < W; x++)
			{
				const float v = peak * (float)x / (float)(W - 1);
				for (int y = 0; y < H; y++)
					put(x, y, v * tintR, v * tintG, v * tintB);
			}
			break;
		}

		case HDR_PATTERN_HIGHLIGHT_DISC:
		{
			const float cx = (float)W * 0.5f;
			const float cy = (float)H * 0.5f;
			const float radius = (float)((W < H) ? W : H) * 0.45f;
			for (int y = 0; y < H; y++)
			{
				for (int x = 0; x < W; x++)
				{
					const float dx = (float)x - cx;
					const float dy = (float)y - cy;
					const float d = sqrtf(dx * dx + dy * dy) / radius;
					float t = 1.0f - d;
					if (t < 0.0f) t = 0.0f;
					// Cubed, so the bright core is small. A linear falloff
					// spreads the peak over most of the disc, which is exactly
					// the case a panel's power limiter handles WELL and a real
					// specular highlight is not.
					const float v = baseLevel + (peak - baseLevel) * t * t * t;
					put(x, y, v * tintR, v * tintG, v * tintB);
				}
			}
			break;
		}

		case HDR_PATTERN_GREY_STEPS:
		{
			fillRect(0, 0, W, H, 0.0f, 1.0f, 1.0f, 1.0f);
			const int mid = H / 2;

			// Top: the ordinary SDR ramp, 0 .. 1.0 in eleven steps.
			const int sdrSteps = 11;
			for (int i = 0; i < sdrSteps; i++)
			{
				const int x0 = (int)((float)W * (float)i / (float)sdrSteps);
				const int x1 = (int)((float)W * (float)(i + 1) / (float)sdrSteps);
				fillRect(x0, 0, x1, mid, (float)i / (float)(sdrSteps - 1), 1.0f, 1.0f, 1.0f);
			}

			// Bottom: white and above.
			for (int i = 0; i < steps; i++)
			{
				const int x0 = (int)((float)W * (float)i / (float)steps);
				const int x1 = (int)((float)W * (float)(i + 1) / (float)steps);
				fillRect(x0, mid, x1, H, stepValue(i), 1.0f, 1.0f, 1.0f);
			}
			break;
		}

		case HDR_PATTERN_COLOR_VOLUME:
		{
			fillRect(0, 0, W, H, baseLevel * 0.25f, 1.0f, 1.0f, 1.0f);
			for (int c = 0; c < 7; c++)
			{
				const int x0 = (int)((float)W * (float)c / 7.0f);
				const int x1 = (int)((float)W * (float)(c + 1) / 7.0f);
				for (int i = 0; i < steps; i++)
				{
					const int y0 = (int)((float)H * (float)i / (float)steps);
					const int y1 = (int)((float)H * (float)(i + 1) / (float)steps);
					fillRect(x0, y0, x1, y1, stepValue(i),
							 kVolumeColumns[c][0], kVolumeColumns[c][1], kVolumeColumns[c][2]);
				}
			}
			break;
		}

		case HDR_PATTERN_CHECKERBOARD:
		{
			const int tile = (H / 8 < 4) ? 4 : H / 8;
			for (int y = 0; y < H; y++)
			{
				for (int x = 0; x < W; x++)
				{
					const bool hot = (((x / tile) + (y / tile)) & 1) != 0;
					const float v = hot ? peak : 1.0f;
					put(x, y, v * tintR, v * tintG, v * tintB);
				}
			}
			break;
		}

		default:
			break;
	}
}

void CViewHdrTest::ComputeStats()
{
	memset(histogramCounts, 0, sizeof(histogramCounts));

	statsPeakLinear = 0.0f;
	statsMinLinear = 1e30f;
	statsMeanLinear = 0.0f;
	statsFractionAboveWhite = 0.0f;

	const size_t pixels = (size_t)patternWidth * (size_t)patternHeight;
	if (pixels == 0 || linearRGB.size() < pixels * 3)
		return;

	const float binScale = (float)(kHistogramBins - 1) /
						   (float)(kHistogramMaxStop - kHistogramMinStop);
	double sum = 0.0;
	size_t aboveWhite = 0;

	for (size_t i = 0; i < pixels; i++)
	{
		const float r = linearRGB[i * 3 + 0];
		const float g = linearRGB[i * 3 + 1];
		const float b = linearRGB[i * 3 + 2];

		// The MAX channel, not luminance. Clipping is a per-channel event: a
		// saturated red at four stops above white clips long before its
		// luminance suggests it would.
		const float m = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);

		if (m > statsPeakLinear) statsPeakLinear = m;
		if (m < statsMinLinear)  statsMinLinear = m;
		sum += (double)m;
		if (m > 1.0f) aboveWhite++;

		int bin = (int)((SafeLog2(m) - (float)kHistogramMinStop) * binScale + 0.5f);
		if (bin < 0) bin = 0;
		if (bin >= kHistogramBins) bin = kHistogramBins - 1;
		histogramCounts[bin] += 1.0f;
	}

	statsMeanLinear = (float)(sum / (double)pixels);
	statsFractionAboveWhite = (float)aboveWhite / (float)pixels;
	if (statsMinLinear > 1e29f)
		statsMinLinear = 0.0f;
}

void CViewHdrTest::UploadPattern()
{
	ReleaseImage();

	if (gRenderBackend == NULL)
	{
		LOGD("CViewHdrTest::UploadPattern: no render backend, nothing uploaded");
		return;
	}

	const size_t pixels = (size_t)patternWidth * (size_t)patternHeight;
	if (pixels == 0 || linearRGB.size() < pixels * 3)
		return;

	// IMG_TYPE_RGBA_16F unconditionally, even on a backend that cannot upload
	// it. Deciding here would hide the interesting half of the behaviour: the
	// engine's resident-format funnel (CSlrImage::ApplyResidentFormat) is what
	// tone-maps to 8 bits when the GPU path has no float, and this view reports
	// which answer it gave rather than predicting it.
	CImageData *src = new CImageData(patternWidth, patternHeight, IMG_TYPE_RGBA_16F);
	src->AllocImage(false, true);

	// Both of these travel WITH the pixels and are meaningless apart from them.
	src->floatIsSurfaceEncoded = encodeForSurface;
	src->contentMaxComponent = statsPeakLinear;

	for (int y = 0; y < patternHeight; y++)
	{
		for (int x = 0; x < patternWidth; x++)
		{
			const size_t off = ((size_t)y * (size_t)patternWidth + (size_t)x) * 3;
			src->SetPixelResultFloat(x, y,
									 ToSurface(linearRGB[off + 0], encodeForSurface),
									 ToSurface(linearRGB[off + 1], encodeForSurface),
									 ToSurface(linearRGB[off + 2], encodeForSurface),
									 1.0f);
		}
	}

	// linearScaling = false: a test pattern must not be interpolated across a
	// patch edge, or the boundary a viewer is asked to judge becomes a gradient
	// the sampler invented.
	//
	// bindNow = false posts to the binding queue, which runs at the start of
	// the next frame -- the point at which the previous frame's ImGui draw
	// lists have finished executing. Creating a texture in the middle of this
	// frame's recording is the hazard VID_PostDeleteGLTexture exists for, and
	// there is no reason to take it: one frame of "uploading..." costs nothing.
	CSlrImage *img = new CSlrImage(src, false, false);

	// residentFormat is set synchronously by the funnel inside the constructor,
	// so it can be read now -- the texture itself does not exist until the
	// binding queue runs.
	uploadedFormat = img->residentFormat;
	uploadedBytes = pixels * ((uploadedFormat == RENDER_TEXTURE_RGBA16F) ? 8 : 4);

	// LoadImage() made its own power-of-two copy and owns it. This one is ours.
	delete src;

	patternImage = img;
	patternUploaded = false;
}

void CViewHdrTest::RegenerateNow()
{
	GeneratePatternLinear();
	ComputeStats();
	UploadPattern();
	// The probe holds coordinates into the OLD buffer, which may have just
	// changed size underneath it.
	probeValid = false;
	needsRegenerate = false;
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

void CViewHdrTest::RenderImGui()
{
	PreRenderImGui();

	if (needsRegenerate)
	{
		RegenerateNow();
	}

	if (patternImage != NULL && !patternUploaded && patternImage->TexturePtr() != NULL)
	{
		patternUploaded = true;
	}

	// The headroom trace samples every frame the window is up. It is the only
	// way to SEE macOS ramping EDR headroom over the first seconds instead of
	// granting it at once.
	{
		const float dt = ImGui::GetIO().DeltaTime;
		traceElapsed += (dt > 0.0f && dt < 1.0f) ? dt : 0.0f;
		traceTime[traceHead] = (float)traceElapsed;
		traceHeadroom[traceHead] = LiveHeadroom();
		traceHead = (traceHead + 1) % kTraceLength;
		if (traceCount < kTraceLength)
			traceCount++;
	}

	RenderStatusBanner();
	ImGui::Separator();

	ImGui::BeginChild("hdrControls", ImVec2(330.0f, 0.0f), ImGuiChildFlags_Borders);
	RenderControlsColumn();
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("hdrContent", ImVec2(0.0f, 0.0f));
	if (ImGui::BeginTabBar("hdrTabs"))
	{
		if (ImGui::BeginTabItem("Pattern"))
		{
			RenderPatternTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Display"))
		{
			RenderDisplayTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Statistics"))
		{
			RenderStatsTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::EndChild();

	PostRenderImGui();
}

// ---------------------------------------------------------------------------
// The banner: one line, and it must never overstate the case
// ---------------------------------------------------------------------------

void CViewHdrTest::RenderStatusBanner()
{
	const bool extended = SurfaceIsExtendedRange();
	const float headroom = LiveHeadroom();
	const bool requested = VID_IsHdrRequested();

	// Three states, not two. "Requested but not granted" is the common one on
	// macOS for the first seconds of a session and after a display change, and
	// reporting it as plain "off" is how people conclude the feature is broken.
	ImVec4 colour;
	const char *headline;
	if (extended && headroom > 1.001f)
	{
		colour = ImVec4(0.35f, 0.85f, 0.40f, 1.0f);
		headline = "HDR ACTIVE";
	}
	else if (extended)
	{
		colour = ImVec4(0.95f, 0.75f, 0.25f, 1.0f);
		headline = "HDR SURFACE, NO HEADROOM YET";
	}
	else
	{
		colour = ImVec4(0.85f, 0.45f, 0.45f, 1.0f);
		headline = "HDR INACTIVE";
	}

	ImGui::PushStyleColor(ImGuiCol_Text, colour);
	ImGui::TextUnformatted(headline);
	ImGui::PopStyleColor();
	ImGui::SameLine();

	const char *backendName = VID_GetCurrentRenderBackendName();
	ImGui::Text("| %s surface, %s | headroom %.3fx (%+.2f stops) | requested: %s",
				extended ? "extended-range" : "SDR",
				SurfaceIsLinear() ? "LINEAR" : "encoded",
				headroom, SafeLog2(headroom),
				requested ? "yes" : "no");
	ImGui::SameLine();
	ImGui::TextDisabled("[%s]", backendName != NULL ? backendName : "?");

	if (!extended)
	{
		ImGui::TextDisabled(
			"Above-white values cannot survive to this surface. The patterns below "
			"still generate correctly; they will clip at 1.0 on the way out.");
	}
	else if (headroom <= 1.001f)
	{
		ImGui::TextDisabled(
			"macOS grants EDR headroom lazily -- it reads 1.0 for the first seconds "
			"and then ramps. Watch the Display tab's trace before concluding anything.");
	}
}

// ---------------------------------------------------------------------------
// Controls
// ---------------------------------------------------------------------------

void CViewHdrTest::RenderControlsColumn()
{
	// The primary action sits at the TOP rather than under four collapsing
	// headers. Every control below regenerates on change, so this is only the
	// manual refresh -- but on a short window the bottom of this column is
	// below the fold, and a control you have to scroll to find is one you will
	// not find. (Measured, not guessed: the ImGui test engine could not reach
	// the button here at all while it sat at the bottom.)
	if (ImGui::Button("Regenerate", ImVec2(-1.0f, 0.0f)))
		needsRegenerate = true;
	ImGui::SetItemTooltip("Regenerate the pattern and re-upload the texture.\n"
						  "Every control below does this automatically on change.");
	ImGui::Separator();

	// "Test pattern", not "Pattern": the content pane's first TAB is already
	// called Pattern, and two items with one label in one window make every
	// path that names it ambiguous -- for a test driver and for a person
	// reading a bug report alike.
	if (ImGui::CollapsingHeader("Test pattern", ImGuiTreeNodeFlags_DefaultOpen))
	{
		int p = (int)pattern;
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::Combo("##pattern", &p, kPatternNames, HDR_PATTERN_COUNT))
		{
			pattern = (EHdrTestPattern)p;
			needsRegenerate = true;
		}
		ImGui::TextWrapped("%s", kPatternHelp[pattern]);

		int w = patternWidth, h = patternHeight;
		ImGui::SetNextItemWidth(90.0f);
		if (ImGui::InputInt("W", &w, 0)) { patternWidth  = (w < 16) ? 16 : ((w > 2048) ? 2048 : w); needsRegenerate = true; }
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.0f);
		if (ImGui::InputInt("H", &h, 0)) { patternHeight = (h < 16) ? 16 : ((h > 2048) ? 2048 : h); needsRegenerate = true; }
		ImGui::SetItemTooltip("Pattern resolution. Bigger is not better here -- what matters is\n"
							  "that a patch is large enough to judge without the eye averaging\n"
							  "it against its neighbour.");
	}

	if (ImGui::CollapsingHeader("Levels", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// STOPS, not a linear multiplier. Displays are specified in stops of
		// headroom, and a linear slider would spend nine tenths of its travel
		// in a range no panel on the market can show.
		if (ImGui::SliderFloat("Peak (stops)", &peakStops, 0.0f, 6.0f, "%.2f"))
			needsRegenerate = true;
		// SetItemTooltip attaches to the LAST item, so it has to come before the
		// SameLine()/TextDisabled() readout -- otherwise it lands on the label.
		ImGui::SetItemTooltip("Peak of the pattern in stops above SDR white.\n"
							  "0 = ordinary white, 3 = 8x white.");
		ImGui::SameLine();
		ImGui::TextDisabled("= %.2fx", powf(2.0f, peakStops));

		if (ImGui::SliderInt("Steps", &stepCount, 2, 16))
			needsRegenerate = true;

		if (ImGui::SliderFloat("Surround", &baseLevel, 0.0f, 1.0f, "%.3f"))
			needsRegenerate = true;
		ImGui::SetItemTooltip("Linear level of the background the patches are judged against.\n"
							  "0.18 is the photographic mid-grey.");

		float tint[3] = { tintR, tintG, tintB };
		if (ImGui::ColorEdit3("Tint", tint, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_NoInputs))
		{
			tintR = tint[0]; tintG = tint[1]; tintB = tint[2];
			needsRegenerate = true;
		}
		ImGui::SetItemTooltip("Multiplies the pattern. A pure primary at high peak is the\n"
							  "hardest case for a wide-gamut panel.");
	}

	if (ImGui::CollapsingHeader("Encoding", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Checkbox("Encode for surface (sRGB)", &encodeForSurface))
			needsRegenerate = true;
		ImGui::TextWrapped(
			"ON is correct for this engine. Both surfaces carry extended-sRGB "
			"ENCODED values, and RGBA16F has no hardware sRGB-decode variant, so "
			"the pattern must be encoded before it becomes half floats.\n\n"
			"Turn it OFF to see the failure directly: mid-grey drops to 0.21 and "
			"every above-white patch lands far too dim. That is the same mistake "
			"as passing linear pixels to an encoded surface in a real app.");
	}

	// No "/" in this label. It is the path separator the ImGui test engine uses,
	// so "Surface / EDR metadata" is unaddressable from a test.
	if (ImGui::CollapsingHeader("EDR metadata"))
	{
		const bool hasMetadata = (gRenderBackend != NULL) && gRenderBackend->GetSurfaceHasEdrMetadata();
		ImGui::Text("Surface carries metadata: %s", hasMetadata ? "yes" : "no");

		// Disabled on a surface that has no such concept -- here the LIVE
		// surface is the right thing to ask, unlike the HDR preference above:
		// this control acts on the surface that exists right now, not on what
		// the next launch will build.
		const bool surfaceTakesMetadata = SurfaceIsExtendedRange();
		ImGui::BeginDisabled(!surfaceTakesMetadata);

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::SliderFloat("##edrpeak", &edrMetadataPeak, 1.0f, 64.0f, "maxComponent %.2f",
						   ImGuiSliderFlags_Logarithmic);

		if (ImGui::Button("Apply") && gRenderBackend != NULL)
			gRenderBackend->SetSurfaceEdrMetadata(edrMetadataPeak);
		ImGui::SameLine();
		if (ImGui::Button("Clear") && gRenderBackend != NULL)
			gRenderBackend->SetSurfaceEdrMetadata(1.0f);
		ImGui::SameLine();
		if (ImGui::Button("Use pattern peak") && gRenderBackend != NULL)
			gRenderBackend->SetSurfaceEdrMetadata(statsPeakLinear);

		ImGui::EndDisabled();

		if (!surfaceTakesMetadata)
		{
			ImGui::TextDisabled("The running surface is not extended-range, so these are no-ops "
								"on this backend.");
		}

		ImGui::TextWrapped(
			"MEASURED, and counter-intuitive: on an EXTENDED-RANGE surface this "
			"metadata is actively harmful. It makes above-white and SDR white "
			"render IDENTICALLY, because HDR10 describes PQ content and an "
			"extended-sRGB buffer is not PQ. The Metal backend therefore applies "
			"it only on a PQ layer. Apply it here and watch the split pattern "
			"collapse -- that is the bug, reproduced on demand.");
	}

}

// ---------------------------------------------------------------------------
// Pattern tab
// ---------------------------------------------------------------------------

void CViewHdrTest::RenderPatternTab()
{
	RenderPatternImageAndProbe();
	ImGui::Separator();
	RenderLevelLegend();
}

void CViewHdrTest::RenderPatternImageAndProbe()
{
	if (patternImage == NULL || !patternUploaded)
	{
		ImGui::TextDisabled("Uploading pattern...");
		return;
	}

	const float avail = ImGui::GetContentRegionAvail().x;
	float scale = (patternWidth > 0) ? (avail / (float)patternWidth) : 1.0f;
	if (scale > 2.0f) scale = 2.0f;
	if (scale < 0.05f) scale = 0.05f;

	const ImVec2 drawSize((float)patternWidth * scale, (float)patternHeight * scale);

	// The image is power-of-two padded by CSlrImage::LoadImage, so the UVs must
	// come from the image rather than being assumed to be (0,0)-(1,1).
	const ImVec2 uv0(patternImage->defaultTexStartX, patternImage->defaultTexStartY);
	const ImVec2 uv1(patternImage->defaultTexEndX,   patternImage->defaultTexEndY);

	const ImVec2 origin = ImGui::GetCursorScreenPos();
	ImGui::Image(patternImage->TexturePtr(), drawSize, uv0, uv1);

	const bool hovered = ImGui::IsItemHovered();
	ImDrawList *dl = ImGui::GetWindowDrawList();

	// Overlays that need to know where the patches are. Drawn with ImGui (so
	// SDR, so always legible) ON TOP of the float texture, rather than baked
	// into the pattern where a bright label would itself become HDR content.
	const ImU32 lineCol = IM_COL32(255, 255, 255, 90);
	const ImU32 warnCol = IM_COL32(255, 90, 90, 220);
	const float headroom = LiveHeadroom();
	const int steps = (stepCount < 2) ? 2 : stepCount;

	if (pattern == HDR_PATTERN_STOP_LADDER || pattern == HDR_PATTERN_COLOR_VOLUME ||
		pattern == HDR_PATTERN_GREY_STEPS)
	{
		for (int i = 0; i < steps; i++)
		{
			const float v = powf(2.0f, peakStops * (float)i / (float)(steps - 1));
			char label[32];
			snprintf(label, sizeof(label), "%.2fx", v);

			if (pattern == HDR_PATTERN_COLOR_VOLUME)
			{
				const float y = origin.y + drawSize.y * ((float)i + 0.5f) / (float)steps;
				dl->AddText(ImVec2(origin.x + 4.0f, y - 7.0f),
							(v > headroom) ? warnCol : IM_COL32(255, 255, 255, 220), label);
			}
			else
			{
				const float x = origin.x + drawSize.x * ((float)i + 0.5f) / (float)steps;
				const float y = (pattern == HDR_PATTERN_GREY_STEPS)
					? (origin.y + drawSize.y - 18.0f)
					: (origin.y + 4.0f);
				dl->AddText(ImVec2(x - 14.0f, y),
							(v > headroom) ? warnCol : IM_COL32(0, 0, 0, 220), label);
			}
		}
	}

	if (pattern == HDR_PATTERN_LINEAR_RAMP)
	{
		// Where the ramp crosses SDR white, and where the display's granted
		// headroom runs out. Two vertical rules turn "it looks flat somewhere
		// over there" into a measurement.
		const float peak = powf(2.0f, peakStops);
		if (peak > 1.0f)
		{
			const float xWhite = origin.x + drawSize.x * (1.0f / peak);
			dl->AddLine(ImVec2(xWhite, origin.y), ImVec2(xWhite, origin.y + drawSize.y), lineCol, 1.0f);
			dl->AddText(ImVec2(xWhite + 3.0f, origin.y + 3.0f), IM_COL32(0, 0, 0, 220), "1.0");
		}
		if (headroom > 1.0f && headroom < peak)
		{
			const float xHead = origin.x + drawSize.x * (headroom / peak);
			dl->AddLine(ImVec2(xHead, origin.y), ImVec2(xHead, origin.y + drawSize.y), warnCol, 2.0f);
			dl->AddText(ImVec2(xHead + 3.0f, origin.y + 20.0f), warnCol, "headroom");
		}
	}

	// ---- probe ----
	if (hovered)
	{
		const ImVec2 mouse = ImGui::GetMousePos();
		const int px = (int)((mouse.x - origin.x) / scale);
		const int py = (int)((mouse.y - origin.y) / scale);
		float r, g, b;
		if (GetPatternPixelLinear(px, py, &r, &g, &b))
		{
			probeValid = true;
			probeX = px; probeY = py;
			probeR = r; probeG = g; probeB = b;
		}
	}

	if (probeValid)
	{
		const float m = (probeR > probeG) ? ((probeR > probeB) ? probeR : probeB)
										  : ((probeG > probeB) ? probeG : probeB);
		ImGui::Text("Probe (%d,%d)  linear R %.4f  G %.4f  B %.4f", probeX, probeY, probeR, probeG, probeB);
		ImGui::Text("  encoded  R %.4f  G %.4f  B %.4f   |   %+0.2f stops vs white   |   ~%.0f nits (assumed)",
					ToSurface(probeR, encodeForSurface),
					ToSurface(probeG, encodeForSurface),
					ToSurface(probeB, encodeForSurface),
					SafeLog2(m), m * assumedSdrWhiteNits);
		if (m > LiveHeadroom())
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "  CLIPS");
		}
	}
	else
	{
		ImGui::TextDisabled("Hover the pattern to probe a pixel.");
	}
}

void CViewHdrTest::RenderLevelLegend()
{
	const int steps = (stepCount < 2) ? 2 : stepCount;
	const float headroom = LiveHeadroom();

	ImGui::SetNextItemWidth(160.0f);
	ImGui::InputFloat("SDR white (nits, assumed)", &assumedSdrWhiteNits, 0.0f, 0.0f, "%.0f");
	ImGui::SetItemTooltip(
		"An ASSUMPTION, used only for the nits column.\n"
		"203 is BT.2408 reference white. How many nits the display actually\n"
		"gives our 1.0 is the OS's decision -- display brightness on macOS,\n"
		"the SDR slider on Windows -- and neither reports it as a nit count.");

	if (ImGui::BeginTable("levels", 5,
						  ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
						  ImGuiTableFlags_SizingFixedFit))
	{
		ImGui::TableSetupColumn("Step");
		ImGui::TableSetupColumn("Linear");
		ImGui::TableSetupColumn("Stops");
		ImGui::TableSetupColumn("Encoded");
		ImGui::TableSetupColumn("~Nits / verdict");
		ImGui::TableHeadersRow();

		for (int i = 0; i < steps; i++)
		{
			const float v = powf(2.0f, peakStops * (float)i / (float)(steps - 1));
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (pattern == HDR_PATTERN_COLOR_VOLUME)
				ImGui::Text("row %d", i);
			else
				ImGui::Text("%d", i);
			ImGui::TableNextColumn(); ImGui::Text("%.4f", v);
			ImGui::TableNextColumn(); ImGui::Text("%+.2f", SafeLog2(v));
			ImGui::TableNextColumn(); ImGui::Text("%.4f", ToSurface(v, encodeForSurface));
			ImGui::TableNextColumn();
			if (v > headroom * 1.001f)
				ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%.0f  CLIPPED", v * assumedSdrWhiteNits);
			else
				ImGui::Text("%.0f", v * assumedSdrWhiteNits);
		}
		ImGui::EndTable();
	}

	if (pattern == HDR_PATTERN_COLOR_VOLUME)
	{
		ImGui::TextDisabled("Columns, left to right: %s %s %s %s %s %s %s",
							kVolumeColumnNames[0], kVolumeColumnNames[1], kVolumeColumnNames[2],
							kVolumeColumnNames[3], kVolumeColumnNames[4], kVolumeColumnNames[5],
							kVolumeColumnNames[6]);
	}
}

// ---------------------------------------------------------------------------
// Display tab
// ---------------------------------------------------------------------------

void CViewHdrTest::RenderDisplayTab()
{
	// LIFETIME: VID_GetPersistedRenderBackend(), VID_GetPreferredRenderBackend()
	// and VID_GetEffectiveRenderBackendSelection() share ONE per-thread buffer,
	// so calling a second one silently overwrites the first. Copy immediately.
	const std::string persistedBackend = VID_GetPersistedRenderBackend();
	const std::string effectiveBackend = VID_GetEffectiveRenderBackendSelection();
	const std::string currentSelection = VID_GetCurrentRenderBackendSelection();

	if (ImGui::CollapsingHeader("HDR preference", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// ---------------------------------------------------------------
		// The control is DISABLED when the backend cannot carry HDR at all.
		//
		// Keyed on the PERSISTED backend, never the running one, and the engine
		// header is explicit about why: a backend switch needs a restart, so a
		// live-backend query would grey this out for somebody who has just
		// chosen Metal and leave it enabled after they chose OpenGL. What the
		// user picks here has to line up with what they picked next door.
		//
		// GREYED, not hidden -- the same principle as the AI submenu. A control
		// that vanishes leaves no way to learn why; one that is visible and
		// disabled can say "you are on OpenGL" and point at the fix.
		// ---------------------------------------------------------------
		const bool backendCanHdr = VID_IsRenderBackendHdrCapable(persistedBackend.c_str());

		// A SECOND, narrower gate, and only on "On": the engine exposes
		// VID_IsAnyDisplayHdrCapable() for this radio specifically. It is a live
		// uncached poll, so plugging an HDR display in mid-session enables the
		// radio on the very next redraw. "auto" and "off" stay available --
		// asking for auto on an SDR display is a legitimate thing to persist.
		const bool anyDisplayCanHdr = VID_IsAnyDisplayHdrCapable();

		// The PERSISTED mode, never VID_IsHdrRequested(). That one checks the
		// command line first, so under --hdr= the UI would show the flag's
		// value, and every click would appear to do nothing.
		const char *mode = VID_GetPersistedHdrMode();
		const char *modes[3] = { "auto", "on", "off" };

		ImGui::BeginDisabled(!backendCanHdr);
		for (int i = 0; i < 3; i++)
		{
			if (i > 0) ImGui::SameLine();
			const bool isOnRadio = (strcmp(modes[i], "on") == 0);
			ImGui::BeginDisabled(isOnRadio && !anyDisplayCanHdr);

			const bool selected = (mode != NULL && strcmp(mode, modes[i]) == 0);
			if (ImGui::RadioButton(modes[i], selected))
				VID_SetHdrMode(modes[i]);

			ImGui::EndDisabled();
		}
		ImGui::EndDisabled();

		if (!backendCanHdr)
		{
			ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f),
							   "%s cannot drive an HDR surface. Choose a backend tagged HDR in "
							   "Settings > Renderer and restart; the HDR preference is disabled "
							   "until then.",
							   VID_GetRenderBackendDisplayName(persistedBackend.c_str()));
		}
		else if (!anyDisplayCanHdr)
		{
			ImGui::TextDisabled("No attached display reports HDR capability, so \"on\" would have "
								"nothing to grant. This is a live poll -- plug one in and the "
								"radio enables itself.");
		}

		if (VID_IsHdrOverriddenByCommandLine())
		{
			ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f),
							   "--hdr on the command line is winning this run; "
							   "the saved choice above takes effect without it.");
		}
		ImGui::TextDisabled("Takes effect on the next launch -- the surface's colour space is "
							"chosen when the layer is created.");

		ImGui::Separator();
		ImGui::Text("Persisted backend:  %s", persistedBackend.c_str());
		ImGui::Text("Next launch will use: %s", effectiveBackend.c_str());
		ImGui::Text("Running now:         %s (%s)",
					VID_GetCurrentRenderBackendName(), currentSelection.c_str());

		// Ask about the PERSISTED backend, never the live one: a switch needs a
		// restart, so a live query would grey this out for somebody who has
		// just chosen a capable backend.
		const bool capable = VID_IsRenderBackendHdrCapable(persistedBackend.c_str());
		ImGui::Text("Persisted backend can drive an HDR surface: %s", capable ? "yes" : "no");

		const char *names[8];
		const int count = VID_GetAvailableRenderBackends(names, 8);
		ImGui::Text("Available backends here:");
		for (int i = 0; i < count; i++)
		{
			ImGui::SameLine();
			ImGui::Text("%s%s", VID_GetRenderBackendDisplayName(names[i]),
						VID_IsRenderBackendHdrCapable(names[i]) ? " (HDR)" : "");
		}
	}

	if (ImGui::CollapsingHeader("Live capability", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::BeginTable("caps", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
										 ImGuiTableFlags_SizingFixedFit))
		{
			auto row = [](const char *k, const char *v)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::TextUnformatted(k);
				ImGui::TableNextColumn(); ImGui::TextUnformatted(v);
			};
			auto rowf = [](const char *k, const char *fmt, double v)
			{
				char buf[64]; snprintf(buf, sizeof(buf), fmt, v);
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::TextUnformatted(k);
				ImGui::TableNextColumn(); ImGui::TextUnformatted(buf);
			};

			row("HDR requested (poll)",        VID_IsHdrRequested() ? "yes" : "no");
			row("Any display HDR-capable",     VID_IsAnyDisplayHdrCapable() ? "yes" : "no");
			rowf("Granted headroom (live)",    "%.4f", LiveHeadroom());
			rowf("Max potential headroom",     "%.4f", VID_GetMaxPotentialHdrHeadroom());
			row("Surface format",              (gRenderBackend != NULL &&
												gRenderBackend->GetSurfaceFormat() == RENDER_SURFACE_RGBA16F)
											   ? "RGBA16F" : "RGBA8");
			row("Surface extended range",      SurfaceIsExtendedRange() ? "yes" : "no");
			row("Surface is linear",           SurfaceIsLinear() ? "yes (values are LINEAR)"
																 : "no (values are ENCODED)");
			row("Surface has EDR metadata",    (gRenderBackend != NULL &&
												gRenderBackend->GetSurfaceHasEdrMetadata()) ? "yes" : "no");
			row("Float textures supported",    BackendSupportsFloatTextures() ? "yes" : "no");
			row("Display gamut",               VID_GetDisplayColorGamutName(VID_GetMainDisplayColorGamut()));
			row("Window render gamut",         VID_GetDisplayColorGamutName(VID_GetMainWindowRenderColorGamut()));
			row("Wide gamut display",          VID_IsMainDisplayWideGamut() ? "yes" : "no");
			rowf("Display profile serial",     "%.0f", (double)VID_GetMainDisplayProfileSerial());
			ImGui::EndTable();
		}

		ImGui::TextWrapped(
			"\"Max potential\" is latched once per session and answers the "
			"init-time capability question. \"Granted\" is a poll and is the one "
			"that moves: macOS reads 1.0 until EDR content is on screen and then "
			"ramps over seconds, and keeps moving it with display brightness and "
			"ambient light afterwards.");
	}

	if (ImGui::CollapsingHeader("Headroom over time", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (traceCount > 1 && ImPlot::BeginPlot("##headroom", ImVec2(-1.0f, 200.0f)))
		{
			ImPlot::SetupAxes("seconds", "headroom (x SDR white)");
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 2.0, ImPlotCond_Once);

			// The ring buffer is plotted from its OLDEST sample, so the trace
			// scrolls instead of tearing at the write head.
			const int start = (traceCount == kTraceLength) ? traceHead : 0;
			static float xs[kTraceLength];
			static float ys[kTraceLength];
			for (int i = 0; i < traceCount; i++)
			{
				const int idx = (start + i) % kTraceLength;
				xs[i] = traceTime[idx];
				ys[i] = traceHeadroom[idx];
			}
			ImPlot::PlotLine("granted", xs, ys, traceCount);
			ImPlot::EndPlot();
		}
		else
		{
			ImGui::TextDisabled("Collecting samples...");
		}
	}
}

// ---------------------------------------------------------------------------
// Statistics tab
// ---------------------------------------------------------------------------

void CViewHdrTest::RenderStatsTab()
{
	const float headroom = LiveHeadroom();

	if (ImGui::BeginTable("stats", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
									  ImGuiTableFlags_SizingFixedFit))
	{
		auto rowf = [](const char *k, const char *fmt, double v)
		{
			char buf[64]; snprintf(buf, sizeof(buf), fmt, v);
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::TextUnformatted(k);
			ImGui::TableNextColumn(); ImGui::TextUnformatted(buf);
		};
		auto rows = [](const char *k, const char *v)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::TextUnformatted(k);
			ImGui::TableNextColumn(); ImGui::TextUnformatted(v);
		};

		rowf("Pattern peak (linear)",   "%.4f", statsPeakLinear);
		rowf("Pattern peak (stops)",    "%+.2f", SafeLog2(statsPeakLinear));
		rowf("Pattern min (linear)",    "%.4f", statsMinLinear);
		rowf("Pattern mean (linear)",   "%.4f", statsMeanLinear);
		rowf("Pixels above white",      "%.2f %%", statsFractionAboveWhite * 100.0);
		rows("Pixels surface-encoded",  encodeForSurface ? "yes" : "no (deliberately wrong)");

		// The interesting one. RGBA8 here means the resident-format funnel
		// tone-mapped the pattern at upload because the backend has no float
		// texture path -- the above-white values are gone before the surface
		// ever sees them, whatever the surface can do.
		rows("Resident texture format",
			 (uploadedFormat == RENDER_TEXTURE_RGBA16F)
			 ? "RGBA16F -- above-white survived"
			 : "RGBA8 -- funnel tone-mapped at upload");
		rowf("Texture payload (KB)",    "%.0f", (double)uploadedBytes / 1024.0);
		rowf("Granted headroom",        "%.4f", headroom);
		ImGui::EndTable();
	}

	if (statsPeakLinear > headroom * 1.001f)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
						   "Pattern peak exceeds granted headroom by %.2f stops -- "
						   "the top of this pattern cannot be shown on this display right now.",
						   SafeLog2(statsPeakLinear) - SafeLog2(headroom));
	}
	else if (statsPeakLinear > 1.001f)
	{
		ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.40f, 1.0f),
						   "Pattern fits inside the granted headroom with %.2f stops to spare.",
						   SafeLog2(headroom) - SafeLog2(statsPeakLinear));
	}

	ImGui::Separator();

	if (ImPlot::BeginPlot("Value distribution", ImVec2(-1.0f, 240.0f)))
	{
		ImPlot::SetupAxes("stops above SDR white", "pixels");
		ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);
		ImPlot::PlotBars("pixels", histogramStops, histogramCounts, kHistogramBins,
						 (double)(kHistogramMaxStop - kHistogramMinStop) / (double)kHistogramBins);
		ImPlot::EndPlot();
	}

	ImGui::TextDisabled(
		"Bins are log2 of the MAX channel, not luminance: clipping is a "
		"per-channel event, and a saturated primary clips well before its "
		"luminance says it should.");
}
