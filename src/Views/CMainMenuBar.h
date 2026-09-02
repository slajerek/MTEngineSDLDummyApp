#ifndef _GUI_MAIN_MENU_BAR_
#define _GUI_MAIN_MENU_BAR_

#include "SYS_Defs.h"
#include "CSlrKeyboardShortcuts.h"
#include <vector>

class CByteBuffer;
class CLayoutData;
class CViewDummyAppMain;

class CMainMenuBar : public CSlrKeyboardShortcutCallback
{
public:
	CMainMenuBar(CViewDummyAppMain *viewMain);
	void RenderImGui();

	//
	virtual bool ProcessKeyboardShortcut(u32 zone, u8 actionType, CSlrKeyboardShortcut *keyboardShortcut);
	CSlrKeyboardShortcut *kbsQuitApplication;
	
	// save workspace layout
	CLayoutData *layoutData;
	char layoutName[128];
	bool doNotUpdateViewsPosition;
	bool waitingForNewLayoutKeyShortcut;
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);

private:
	// Settings > Theme: lists registered themes + the engine's built-in styles.
	void RenderThemeMenu();

	// Settings > Renderer: the draw backend (OpenGL / Metal / Direct3D 11).
	void RenderRendererMenu();

	// Settings > GUI Scale. Its own knob rather than the theme's, because a
	// host with no active theme still needs it -- see the implementation.
	void RenderGuiScaleMenu();
	void ApplyGuiScale(float scale);

	CViewDummyAppMain *viewMain;
};

#endif
