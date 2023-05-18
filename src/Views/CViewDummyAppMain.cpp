#include "CViewDummyAppMain.h"
#include "CGuiMain.h"
#include "CMainMenuBar.h"

CViewDummyAppMain::CViewDummyAppMain(float posX, float posY, float sizeX, float sizeY)
: CGuiView("CViewDummyAppMain", posX, posY, -1, sizeX, sizeY)
{
	mainMenuBar = new CMainMenuBar();
	
	imGuiNoWindowPadding = false;
	imGuiNoScrollbar = false;
	
	midiIn = new CMidiInKeyboard(0, this);;
}

void CViewDummyAppMain::RenderImGui()
{
	if (guiMain->IsViewFullScreen() == false)
	{
		mainMenuBar->RenderImGui();
	}

	PreRenderImGui();

	
	ImGui::Text("Press ENTER to toggle full screen");

	static bool clicked = false;
	if (ImGui::Button("CLICK ME"))
	{
		clicked = true;
	}

	//
	if (clicked)
	{
		ImGui::SameLine();
		ImGui::Text("CLICKED");
	}
	
	//
	static int count = 0;

	if (count > 100)
	{
		static int a = 0;
		static int b = 0;
		static int c = 0;

		ImGui::InputInt("A", &a);
		ImGui::InputInt("B", &b);
		ImGui::InputInt("C", &c);

		ImGui::Text("Value of a is %d", a);
		ImGui::Text("Value of b is %d", b);
		ImGui::Text("Value of c is %d", c);
	}

	count++;
	
	ImDrawList *drawList = ImGui::GetWindowDrawList();

	ImVec2 p1(this->posX + this->sizeX/2.0f, this->posY + this->sizeY/2.0f);
	ImVec2 p2(guiMain->mousePosX, guiMain->mousePosY);

	drawList->AddLine(p1, p2, ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
		
	PostRenderImGui();
}

bool CViewDummyAppMain::DoTap(float x, float y)
{
	LOGD("DoTap: x=%f y=%f", x, y);
	return CGuiView::DoTap(x, y);
}

bool CViewDummyAppMain::KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	LOGD("KeyDown: keyCode=%d %d %d %d %d", keyCode, isShift, isAlt, isControl, isSuper);
	
	if (mainMenuBar->KeyDown(keyCode, isShift, isAlt, isControl, isSuper))
		return true;
	
	if (keyCode == MTKEY_ENTER)
	{
		if (guiMain->IsViewFullScreen() == false)
		{
			// go full screen
			guiMain->SetViewFullScreen(ViewEnterFullScreen, this, 640, 480);
		}
		else
		{
			// leave full screen
			guiMain->SetViewFullScreen(ViewLeaveFullScreen, NULL);
		}
		
		return true;
	}
	
	return false;
}

void CViewDummyAppMain::MidiInKeyboardCallbackNoteOn(int channel, int key, int pressure)
{
	LOGD("MidiInKeyboardCallbackNoteOn: channel=%d key=%d pressure=%d", channel, key, pressure);
}

void CViewDummyAppMain::MidiInKeyboardCallbackNoteOff(int channel, int key, int pressure)
{
	LOGD("MidiInKeyboardCallbackNoteOff: channel=%d key=%d pressure=%d", channel, key, pressure);
}

void CViewDummyAppMain::MidiInKeyboardCallbackControlChange(int knobNum, int value)
{
	LOGD("MidiInKeyboardCallbackControlChange: knobNum=%d value=%d", knobNum, value);
}

void CViewDummyAppMain::MidiInKeyboardCallbackPitchBend(int value)
{
	LOGD("MidiInKeyboardCallbackPitchBend: value=%d", value);
}
