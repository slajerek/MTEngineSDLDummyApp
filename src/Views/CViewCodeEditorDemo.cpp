#include "CViewCodeEditorDemo.h"
#include "DummyAppFonts.h"

CViewCodeEditorDemo::CViewCodeEditorDemo(const char *name, float posX, float posY,
										 float posZ, float sizeX, float sizeY)
: CGuiViewCodeEditor(name, posX, posY, posZ, sizeX, sizeY)
{
	SetText(
		"// The engine's code editor, over the vendored ImGuiColorTextEdit.\n"
		"// Selection, multiple cursors, find/replace (Ctrl+F), undo (Ctrl+Z).\n"
		"// The Font combo above switches between the default UI font, the\n"
		"// engine's JetBrains Mono, and Courier Prime Code embedded by this app.\n"
		"#include <cstdio>\n"
		"\n"
		"int main()\n"
		"{\n"
		"\tprintf(\"hello from MTEngineSDL\\n\");\n"
		"\treturn 0;\n"
		"}\n");

	// Deliberately NO ApplyFontChoice() here -- see the header. Views are
	// constructed before LoadFonts(), so every font pointer is still null.
}

void CViewCodeEditorDemo::ApplyFontChoice()
{
	// EDITOR_FONT_COURIER as the fallback, deliberately. CGuiViewCodeEditor's
	// own default is the engine's JetBrains Mono, which is the right default
	// for a wrapper; this EXAMPLE exists to show an app shipping its own font,
	// so it opens in Courier Prime Code until the user chooses otherwise.
	// After any choice, both editor views follow the one shared setting.
	SetFont(gDummyAppFonts.GetEditorFont(
		gDummyAppFonts.GetEditorFontChoice(EDITOR_FONT_COURIER)));
	fontChoiceApplied = true;
}

void CViewCodeEditorDemo::RenderToolbar()
{
	// First frame: fonts exist now, the constructor could not have known them.
	if (!fontChoiceApplied)
		ApplyFontChoice();

	// THIS is what the hook is for.
	if (gDummyAppFonts.RenderEditorFontCombo("Font", EDITOR_FONT_COURIER))
		ApplyFontChoice();
}
