#include "CViewGamepadViewer.h"
#include "GAM_GamePads.h"

using namespace ImGui;

namespace
{
	// The common face/shoulder/dpad buttons -- skips MISC*/paddle/touchpad
	// buttons, which are sparse across controllers and would make the demo
	// noisier without teaching more about the mechanism.
	const SDL_GamepadButton kShownButtons[] = {
		SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
		SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH,
		SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_GUIDE, SDL_GAMEPAD_BUTTON_START,
		SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
		SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
		SDL_GAMEPAD_BUTTON_DPAD_UP, SDL_GAMEPAD_BUTTON_DPAD_DOWN,
		SDL_GAMEPAD_BUTTON_DPAD_LEFT, SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
	};

	const char *const kShownButtonLabels[] = {
		"South", "East", "West", "North", "Back", "Guide", "Start",
		"L Stick", "R Stick", "L Shoulder", "R Shoulder",
		"DPad Up", "DPad Down", "DPad Left", "DPad Right",
	};

	const SDL_GamepadAxis kShownAxes[] = {
		SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY,
		SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY,
		SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
	};

	const char *const kShownAxisLabels[] = {
		"Left X", "Left Y", "Right X", "Right Y", "Left Trigger", "Right Trigger",
	};
}

CViewGamepadViewer::CViewGamepadViewer(const char *name, float posX, float posY, float posZ,
										float sizeX, float sizeY)
: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	imGuiNoWindowPadding = false;
	imGuiNoScrollbar = false;
}

void CViewGamepadViewer::RenderImGui()
{
	PreRenderImGui();

	TextWrapped("Live state from MTEngineSDL's GAM_EnumerateGamepads() + SDL3's "
				"gamepad API. Plug a controller in and it appears below.");
	Separator();

	int numGamepads = 0;
	CGamePad **pads = GAM_EnumerateGamepads(&numGamepads);

	bool anyActive = false;

	for (int i = 0; i < numGamepads; i++)
	{
		CGamePad *pad = pads[i];
		if (pad == NULL || !pad->isActive || pad->sdlGamePad == NULL)
			continue;

		anyActive = true;

		PushID(i);
		Text("%s (%s)", pad->name != NULL ? pad->name : "Unnamed", pad->guid);

		for (size_t b = 0; b < sizeof(kShownButtons) / sizeof(kShownButtons[0]); b++)
		{
			if (b > 0)
				SameLine();

			bool pressed = SDL_GetGamepadButton(pad->sdlGamePad, kShownButtons[b]);
			PushStyleColor(ImGuiCol_Button, pressed
				? ImVec4(0.20f, 0.60f, 0.20f, 1.0f)
				: ImVec4(0.30f, 0.30f, 0.30f, 1.0f));
			Button(kShownButtonLabels[b]);
			PopStyleColor();
		}

		for (size_t a = 0; a < sizeof(kShownAxes) / sizeof(kShownAxes[0]); a++)
		{
			Sint16 raw = SDL_GetGamepadAxis(pad->sdlGamePad, kShownAxes[a]);
			float normalized = (raw + 32768.0f) / 65535.0f; // 0..1 for the bar
			char overlay[32];
			snprintf(overlay, sizeof(overlay), "%d", (int)raw);
			ProgressBar(normalized, ImVec2(-1.0f, 0.0f), overlay);
			SameLine();
			Text("%s", kShownAxisLabels[a]);
		}

		PopID();
		Separator();
	}

	if (!anyActive)
	{
		TextDisabled("No gamepad connected.");
	}

	PostRenderImGui();
}
