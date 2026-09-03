#include "CViewUndoDemo.h"
#include "ImGuiUndo.h"

using namespace ImGui;

CViewUndoDemo::CViewUndoDemo(const char *name, float posX, float posY, float posZ,
							  float sizeX, float sizeY)
: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	imGuiNoWindowPadding = false;
	imGuiNoScrollbar = false;
}

void CViewUndoDemo::RenderImGui()
{
	PreRenderImGui();

	TextWrapped("Edit the fields below, then Undo/Redo. Each widget pushes one "
				"CUndoAction onto a shared CUndoManager.");
	Separator();

	ImGuiUndo::InputInt(&undoMgr, "Demo Int", &demoInt);
	ImGuiUndo::InputText(&undoMgr, "Demo Text", &demoText);
	ImGuiUndo::Checkbox(&undoMgr, "Demo Bool", &demoBool);

	Separator();

	BeginDisabled(!undoMgr.CanUndo());
	if (Button("Undo"))
	{
		undoMgr.PerformUndo();
	}
	EndDisabled();

	SameLine();

	BeginDisabled(!undoMgr.CanRedo());
	if (Button("Redo"))
	{
		undoMgr.PerformRedo();
	}
	EndDisabled();

	SameLine();

	if (Button("Clear History"))
	{
		undoMgr.Clear();
	}

	PostRenderImGui();
}
