#include "DummyInit.h"
#include "DBG_Log.h"
#include "CGuiMain.h"
#include "MT_API.h"
#include "CViewDummyAppMain.h"

void MT_PreInit()
{
}

SDL_Renderer *renderer = NULL;

void MT_PostInit()
{
	LOGM("MT_Init successfull");
	
	// use ImGui window
	CViewDummyAppMain *viewMain = new CViewDummyAppMain(50, 50, 640 + 50, 480 + 50);
	guiMain->SetView(viewMain);
	
	// or use SDL renderer on main window:
	//	SDL_Window *window = VID_GetMainSDLWindow();
	//	renderer = SDL_CreateRenderer(window, -1, 0);

	LOGM("DummyApp initialized");
}

const char *MT_GetMainWindowTitle()
{
	return "DummyApp";
}

void MT_GetDefaultWindowPositionAndSize(int *defaultWindowPosX, int *defaultWindowPosY, int *defaultWindowWidth, int *defaultWindowHeight)
{
	*defaultWindowPosX = SDL_WINDOWPOS_CENTERED;
	*defaultWindowPosY = SDL_WINDOWPOS_CENTERED;
	*defaultWindowWidth = 640;
	*defaultWindowHeight = 480;
}

const char *MT_GetSettingsFolderName()
{
	return "MTEngineSDLDummyApp";
}

void MT_GuiPreInit()
{
}

void MT_Render()
{
//	LOGM("MT_Render");
	
	// skip SDL renderer, use ImGui:
	return;

	// or use SDL renderer on main window:
	SDL_SetRenderDrawColor(renderer, 242, 242, 242, 255);
	SDL_RenderClear(renderer);
	
	SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
	
	static float x1 = 150;
	static float stepX1 = 1;
	static float y1 = 150;
	static float stepY1 = 0.65;

	static float x2 = 70;
	static float stepX2 = 0.45;
	static float y2 = 70;
	static float stepY2 = 1.25;

	x1 += stepX1;
	if (x1 < 50 || x1 > 600)
	{
		stepX1 = -stepX1;
	}
	
	y1 += stepY1;
	if (y1 < 50 || y1 > 400)
	{
		stepY1 = -stepY1;
	}

	x2 += stepX2;
	if (x2 < 50 || x2 > 600)
	{
		stepX2 = -stepX2;
	}
	
	y2 += stepY2;
	if (y2 < 50 || y2 > 400)
	{
		stepY2 = -stepY2;
	}
	
	SDL_RenderDrawLine(renderer, (int)x1, (int)y1, (int)x2, (int)y2);
	SDL_RenderDrawLine(renderer, (int)600-y1, (int)400-x1, (int)600-y2, (int)400-x2);

	SDL_RenderPresent(renderer);
	
	return;
}

void MT_PostRenderEndFrame()
{
}
