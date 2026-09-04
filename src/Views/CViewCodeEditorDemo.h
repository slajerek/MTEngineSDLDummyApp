#ifndef _CViewCodeEditorDemo_h_
#define _CViewCodeEditorDemo_h_

#include "CGuiViewCodeEditor.h"

// The Examples > Code Editor view: the engine's CGuiViewCodeEditor with the
// app's font picker in its toolbar.
//
// This is the wrapper's first caller, and the first use of its one extension
// point. The wrapper draws no toolbar and knows nothing about fonts; this
// subclass adds exactly the row it wants, and nothing else changes.
class CViewCodeEditorDemo : public CGuiViewCodeEditor
{
public:
	CViewCodeEditorDemo(const char *name, float posX, float posY, float posZ,
						float sizeX, float sizeY);

	virtual void RenderToolbar() override;

	// Re-read the shared editor.font setting and hand the font to the wrapper.
	// Public so a test can drive it without a frame. NOT called from the
	// constructor -- fonts do not exist yet then; the first RenderToolbar()
	// applies it.
	void ApplyFontChoice();

private:
	bool fontChoiceApplied = false;
};

#endif
