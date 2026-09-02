#include "CTestFonts.h"
#include "imgui.h"
#include "DBG_Log.h"
#include <cstdio>

CTestFonts::CTestFonts()  {}
CTestFonts::~CTestFonts() {}

void CTestFonts::Run(ITestCallback *callback)
{
	this->callback = callback;
	isRunning = true;
	int stepNum = 1;

	ImGuiIO &io = ImGui::GetIO();

	ImFont *font = io.FontDefault;
	if (font == NULL && io.Fonts->Fonts.Size > 0)
		font = io.Fonts->Fonts[0];

	if (font == NULL)
	{
		TestCompleted(false, "No default font available");
		return;
	}

	// Polish letters that live in Latin Extended-A. ImFont::IsGlyphInFont()
	// reports whether the loaded font actually provides the glyph — false when
	// the app fell back to the built-in ASCII font, which is the bug we guard
	// against.
	struct { ImWchar cp; const char *name; } polish[] = {
		{ 0x0105, "a-ogonek" },  // ą
		{ 0x0107, "c-acute"  },  // ć
		{ 0x0119, "e-ogonek" },  // ę
		{ 0x0142, "l-stroke" },  // ł
		{ 0x0144, "n-acute"  },  // ń
		{ 0x015B, "s-acute"  },  // ś
		{ 0x017A, "z-acute"  },  // ź
		{ 0x017C, "z-dot"    },  // ż
	};

	for (const auto &g : polish)
	{
		if (!font->IsGlyphInFont(g.cp))
		{
			char buf[160];
			snprintf(buf, sizeof(buf), "FAIL: Polish glyph not in font: %s (U+%04X)", g.name, (unsigned)g.cp);
			LOGError("CTestFonts: %s", buf);
			TestCompleted(false, buf);
			return;
		}
		char msg[96];
		snprintf(msg, sizeof(msg), "Polish glyph present: %s (U+%04X)", g.name, (unsigned)g.cp);
		StepCompleted(stepNum++, true, msg);
	}

	// Basic ASCII must still work.
	if (!font->IsGlyphInFont((ImWchar)'A'))
	{
		TestCompleted(false, "FAIL: ASCII 'A' glyph missing");
		return;
	}
	StepCompleted(stepNum++, true, "ASCII 'A' glyph present");

	TestCompleted(true, "Default UI font provides Polish (Latin Extended-A) glyphs");
}

void CTestFonts::Cancel()
{
	isRunning = false;
}
