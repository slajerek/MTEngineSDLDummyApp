#include "CViewShaderToyOutput.h"
#include "CViewShaderToyDemo.h"
#include "CGuiMain.h"

using namespace ImGui;

CViewShaderToyOutput::CViewShaderToyOutput(const char *name, float posX, float posY, float posZ,
										   float sizeX, float sizeY, CViewShaderToyDemo *editorView)
: CGuiView(name, posX, posY, posZ, sizeX, sizeY), editorView(editorView)
{
	// No padding and no scrollbar: the shader is the window's entire contents,
	// and a border of theme-coloured background around it would read as part
	// of the effect.
	imGuiNoWindowPadding = true;
	imGuiNoScrollbar = true;
}

void CViewShaderToyOutput::RenderImGui()
{
	PreRenderImGui();

	if (editorView != NULL && editorView->HasDrawableShader())
	{
		editorView->RenderShaderInto(GetContentRegionAvail());
	}
	else
	{
		// A line of text rather than an empty window: "nothing compiled yet"
		// and "compiled to a black screen" look identical otherwise, and the
		// second is a shader bug the user should be able to tell apart.
		TextDisabled("No compiled shader yet -- press Alt+Enter in the Shader Toy editor.");
	}

	// Right-click anywhere in the window. The engine already implements
	// fullscreen for a view -- CGuiMain::SetViewFullScreen hides every other
	// view and lets CGuiView::PreRenderImGui size this one to the viewport,
	// keeping its aspect ratio -- so this menu only has to ask for it.
	if (BeginPopupContextWindow("##shaderToyOutputMenu"))
	{
		if (guiMain->IsViewFullScreen())
		{
			if (MenuItem("Leave full screen"))
				guiMain->SetViewFullScreen(SetFullScreenMode::ViewLeaveFullScreen, this);
		}
		else
		{
			if (MenuItem("Full screen"))
				guiMain->SetViewFullScreen(SetFullScreenMode::ViewEnterFullScreen, this);
		}
		EndPopup();
	}

	PostRenderImGui();
}
