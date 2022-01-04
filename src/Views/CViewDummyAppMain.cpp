#include "CViewDummyAppMain.h"
#include "CGuiMain.h"
#include "CMainMenuBar.h"

CViewDummyAppMain::CViewDummyAppMain(float posX, float posY, float sizeX, float sizeY)
: CGuiView("CViewDummyAppMain", posX, posY, -1, sizeX, sizeY)
{
	mainMenuBar = new CMainMenuBar();
	
	imGuiNoWindowPadding = false;
	imGuiNoScrollbar = false;
}

void CViewDummyAppMain::RenderImGui()
{
	if (guiMain->IsViewFullScreen() == false)
	{
		mainMenuBar->RenderImGui();
	}

	PreRenderImGui();

	ImGui::Text("Press ENTER to toggle full screen");

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
			guiMain->SetViewFullScreen(this, 640, 480);
		}
		else
		{
			// leave full screen
			guiMain->SetViewFullScreen(NULL);
		}
		
		return true;
	}
	
	return false;
}

