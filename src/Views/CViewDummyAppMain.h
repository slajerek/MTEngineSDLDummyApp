#ifndef _CViewDummyAppMain_h_
#define _CViewDummyAppMain_h_

#include "CGuiView.h"
#include "SYS_Defs.h"

class CMainMenuBar;
class CViewImage;

class CViewDummyAppMain : public CGuiView
{
public:
	CViewDummyAppMain(float posX, float posY, float sizeX, float sizeY);
	virtual void RenderImGui();
	
	CMainMenuBar *mainMenuBar;
	
	virtual bool DoTap(float x, float y);
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);

};

#endif
