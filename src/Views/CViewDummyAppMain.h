#ifndef _CViewDummyAppMain_h_
#define _CViewDummyAppMain_h_

#include "CGuiView.h"
#include "SYS_Defs.h"
#include "CMidiInKeyboard.h"

class CMainMenuBar;
class CViewImage;
class CMidiInKeyboard;

class CViewDummyAppMain : public CGuiView, CMidiInKeyboardCallback
{
public:
	CViewDummyAppMain(float posX, float posY, float sizeX, float sizeY);
	virtual void RenderImGui();
	
	CMainMenuBar *mainMenuBar;
	
	virtual bool DoTap(float x, float y);
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper);

	CMidiInKeyboard *midiIn;
	
	virtual void MidiInKeyboardCallbackNoteOn(int channel, int key, int pressure);
	virtual void MidiInKeyboardCallbackNoteOff(int channel, int key, int pressure);
	virtual void MidiInKeyboardCallbackControlChange(int knobNum, int value);
	virtual void MidiInKeyboardCallbackPitchBend(int value);
};

#endif
