#include "DummyAppFonts.h"
#include "CGuiFontManager.h"
#include "SYS_DefaultConfig.h"
#include "imgui.h"
#include <cstring>

// The whole font, as data. See that file's own header for why it is included
// here rather than compiled as a translation unit of its own.
#include "font_courier_prime_code.cpp"

CDummyAppFonts gDummyAppFonts;

void CDummyAppFonts::LoadEmbeddedFonts(float size)
{
	// The same ranges CGuiFontManager and CViewDummyAppMain::LoadFonts use.
	// Courier Prime Code carries all of Latin Extended-A that matters -- 18/18
	// Polish glyphs, checked with fontTools before it was embedded.
	//
	// STATIC, because ImGui keeps the pointer: ImFontConfig is copied into the
	// atlas, but GlyphRanges is not, and it is read again when the atlas is
	// (re)built.
	static const ImWchar latin_ranges[] = {
		0x0020, 0x00FF,  // Basic Latin + Latin-1 Supplement
		0x0100, 0x017F,  // Latin Extended-A
		0x0180, 0x024F,  // Latin Extended-B
		0x2013, 0x2014,  // en dash, em dash
		0,
	};

	ImFontConfig cfg;
	cfg.PixelSnapH = true;
	cfg.OversampleH = 2;
	cfg.OversampleV = 2;

	fontCourierPrimeCode = ImGui::GetIO().Fonts->AddFontFromMemoryCompressedTTF(
		font_courier_prime_code_compressed_data,
		(int)font_courier_prime_code_compressed_size,
		size, &cfg, latin_ranges);
}

// The persisted vocabulary. Stable strings, not enum ordinals, so reordering
// the enum can never silently change what a saved settings file means.
static const char *kChoiceKeys[]   = { "ui", "mono", "courier" };
static const char *kChoiceLabels[] = { "Default UI font", "JetBrains Mono (engine)", "Courier Prime Code (app)" };
static const int   kChoiceCount    = 3;

EDummyAppEditorFont CDummyAppFonts::GetEditorFontChoice(EDummyAppEditorFont fallback)
{
	if (gApplicationDefaultConfig == NULL)
		return fallback;

	// The sentinel is deliberately not one of the three keys: it tells an
	// UNSET setting apart from a set one, which is what lets two views want
	// different defaults while sharing one choice.
	const char *v = NULL;
	gApplicationDefaultConfig->GetString("editor.font", &v, "");
	for (int i = 0; i < kChoiceCount; i++)
	{
		if (v != NULL && strcmp(v, kChoiceKeys[i]) == 0)
			return (EDummyAppEditorFont)i;
	}
	return fallback;
}

void CDummyAppFonts::SetEditorFontChoice(EDummyAppEditorFont choice)
{
	int i = (int)choice;
	if (i < 0 || i >= kChoiceCount)
		i = (int)EDITOR_FONT_MONO;
	if (gApplicationDefaultConfig != NULL)
		gApplicationDefaultConfig->SetString("editor.font", kChoiceKeys[i]);
}

ImFont *CDummyAppFonts::GetEditorFont(EDummyAppEditorFont choice)
{
	switch (choice)
	{
		case EDITOR_FONT_MONO:    return gGuiFontManager.fontMono;
		case EDITOR_FONT_COURIER: return fontCourierPrimeCode;
		default:                  return nullptr;
	}
}

bool CDummyAppFonts::RenderEditorFontCombo(const char *label, EDummyAppEditorFont fallback)
{
	int current = (int)GetEditorFontChoice(fallback);
	bool changed = false;

	ImGui::SetNextItemWidth(220.0f);
	if (ImGui::BeginCombo(label, kChoiceLabels[current]))
	{
		for (int i = 0; i < kChoiceCount; i++)
		{
			if (ImGui::Selectable(kChoiceLabels[i], i == current))
			{
				SetEditorFontChoice((EDummyAppEditorFont)i);
				changed = true;
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}
