#ifndef _GUI_MAIN_MENU_BAR_
#define _GUI_MAIN_MENU_BAR_

#include "SYS_Defs.h"
#include "CSlrKeyboardShortcuts.h"
#include <vector>

class CByteBuffer;
class CLayoutData;

class CMainMenuBar : public CSlrKeyboardShortcutCallback
{
public:
	CMainMenuBar();
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

};

#endif
