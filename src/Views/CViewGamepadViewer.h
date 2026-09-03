#ifndef _CViewGamepadViewer_h_
#define _CViewGamepadViewer_h_

#include "CGuiView.h"

// ===========================================================================
// Gamepad viewer example
// ===========================================================================
//
// A live view of MTEngineSDL's GAM_GamePads (Core/GamePads/), which is SDL3-
// backed and unconditionally compiled -- no MT_CAP_* guard wires to it today.
// The engine only wraps enumeration and hotplug (GAM_InitGamePads() is already
// called once by the engine's own startup, GAM_RefreshGamePads() by its own
// hotplug handling -- this view must not call either itself); live per-frame
// button/axis state is read straight from SDL3 against the CGamePad's own
// sdlGamePad handle, the same way the engine's own ImGui SDL3 backend does.
//
// Must render sensibly with zero gamepads connected, since most dev/CI
// machines have none.
// ===========================================================================

class CViewGamepadViewer : public CGuiView
{
public:
	CViewGamepadViewer(const char *name, float posX, float posY, float posZ,
						float sizeX, float sizeY);

	virtual void RenderImGui() override;
};

#endif
