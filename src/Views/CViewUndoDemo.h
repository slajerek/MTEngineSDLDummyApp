#ifndef _CViewUndoDemo_h_
#define _CViewUndoDemo_h_

#include "CGuiView.h"
#include "CUndoManager.h"
#include <string>

// ===========================================================================
// Undo/Redo example
// ===========================================================================
//
// Demonstrates MTEngineSDL's CUndoManager plus the drop-in ImGuiUndo widgets
// (Tools/Undo/) against a few trivial fields -- no game/document logic, so the
// undo mechanism itself is what's on screen. CUndoManager has no public API to
// enumerate its history (the vector backing it is private, and
// CUndoAction::GetDescription() is never called by any UI in the engine), so
// this view only offers Undo/Redo/Clear, not a history list.
// ===========================================================================

class CViewUndoDemo : public CGuiView
{
public:
	CViewUndoDemo(const char *name, float posX, float posY, float posZ,
				  float sizeX, float sizeY);

	virtual void RenderImGui() override;

	// Public so CTestAppStartup can push/inspect state directly, matching how
	// it already reaches into viewCamera->visible.
	CUndoManager undoMgr;
	int demoInt = 0;
	std::string demoText = "hero";
	bool demoBool = false;
};

#endif
