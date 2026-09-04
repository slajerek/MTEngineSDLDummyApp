#ifndef _DummyAppFonts_h_
#define _DummyAppFonts_h_

struct ImFont;

// The app's own embedded fonts, and the one editor-font setting the app's two
// code-editor views share.
//
// THIS IS THE WORKED EXAMPLE OF EMBEDDING A FONT IN AN APPLICATION. The engine
// embeds Inter and JetBrains Mono for markdown; an app that wants its own face
// does the same three things, and they are all here:
//
//   1. src/Fonts/font_courier_prime_code.cpp -- the TTF compressed by
//      MTEngineSDL/other/tools/DeployMaker/binary_to_compressed_c, #included
//      by DummyAppFonts.cpp rather than compiled on its own, exactly as
//      CGuiFontManager.cpp includes the engine's;
//   2. LoadEmbeddedFonts() -- AddFontFromMemoryCompressedTTF before the atlas
//      is built, called from CViewDummyAppMain::LoadFonts();
//   3. mtengine-app-licenses.json -- the licence declaration mtcaps folds into
//      the LICENSES.txt every release package ships.
enum EDummyAppEditorFont
{
	EDITOR_FONT_UI = 0,        // whatever font the UI is drawn in
	EDITOR_FONT_MONO = 1,      // the engine's JetBrains Mono
	EDITOR_FONT_COURIER = 2,   // this app's Courier Prime Code
};

class CDummyAppFonts
{
public:
	// Call from LoadFonts(), BEFORE the atlas is built. Loads Courier Prime
	// Code with the same Latin-Extended ranges the UI font uses, so Polish in
	// a shader comment renders in it too.
	void LoadEmbeddedFonts(float size);

	ImFont *fontCourierPrimeCode = nullptr;

	// The one shared editor-font setting. Persisted as "editor.font" in
	// gApplicationDefaultConfig -- SetString writes settings.hjson at once --
	// and read by both editor views, so a choice made in one shows in the
	// other.
	//
	// `fallback` is what a view gets when the user has never chosen: Shader Toy
	// takes the default (the engine's mono, which is also CGuiViewCodeEditor's
	// own default), and the Code Editor example passes EDITOR_FONT_COURIER so
	// that it opens showing the font THIS APP embeds -- which is the entire
	// point of that example. Once the user picks anything, the choice is
	// written and both views follow it.
	EDummyAppEditorFont GetEditorFontChoice(EDummyAppEditorFont fallback = EDITOR_FONT_MONO);
	void SetEditorFontChoice(EDummyAppEditorFont choice);

	// The ImFont for a choice, or nullptr for "use the current font" -- which
	// is what EDITOR_FONT_UI means, and what any choice degrades to when its
	// font was never loaded (an app that skipped LoadMarkdownFonts() has no
	// fontMono, and the combo must still work).
	ImFont *GetEditorFont(EDummyAppEditorFont choice);

	// Draws the combo and writes the setting on change. Returns true when the
	// choice changed this frame. `fallback` is as above: what the combo shows
	// before the user has chosen anything.
	bool RenderEditorFontCombo(const char *label, EDummyAppEditorFont fallback = EDITOR_FONT_MONO);
};

extern CDummyAppFonts gDummyAppFonts;

#endif
