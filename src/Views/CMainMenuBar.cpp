#include "GUI_Main.h"

// The generated capability header, for MT_HAS_CAP() below. Written by
// tools/mtcaps into $MT_OUT/include/ on every build and reached through
// $(MT_CAPS_INCLUDE_DIR), so it resolves identically from ./build-macos.sh and
// from Xcode.
#include "MT_Capabilities.h"
#include "CMainMenuBar.h"
#include "CLayoutManager.h"
#include "CSlrKeyboardShortcuts.h"
#include "CI18nManager.h"
#include "SYS_DefaultConfig.h"
#include "CViewDummyAppMain.h"
#include "CGuiViewUiDebug.h"
#include "CMTThemeRegistry.h"
// MT_kGuiScaleSteps / MT_ThemeClampGuiScale for the GUI Scale menu.
#include "MT_Theme.h"
// MT_SetUiScale / MT_GetUiScale -- the engine owns the scale (see
// MTEngineSDL/docs/hidpi-ui-scaling.md).
#include "MT_UiScale.h"
#include "VID_Main.h"
#include <cstring>
#include <string>

CMainMenuBar::CMainMenuBar(CViewDummyAppMain *viewMain)
: viewMain(viewMain)
{
	layoutData = NULL;
	
	// keyboard shortcuts
#if defined(MACOS)
	kbsQuitApplication = new CSlrKeyboardShortcut(MT_KEYBOARD_SHORTCUT_GLOBAL, "Quit application", 'q', false, false, true, false, this);
#elif defined(LINUX) || defined(WIN32)
	kbsQuitApplication = new CSlrKeyboardShortcut(MT_KEYBOARD_SHORTCUT_GLOBAL, "Quit application", MTKEY_F4, false, true, false, false, this);
#endif
	guiMain->AddKeyboardShortcut(kbsQuitApplication);

	//
	waitingForNewLayoutKeyShortcut = false;
}

void CMainMenuBar::RenderImGui()
{
	static bool openPopupImGuiWorkaround = false;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu(_T("menu.file")))
		{
//			if (ImGui::MenuItem("Open", kbsOpenFile->cstr))
//			{
//
//			}
			
			ImGui::Separator();
			
			if (ImGui::MenuItem(_T("menu.file.quit"), kbsQuitApplication->cstr))
			{
				kbsQuitApplication->Run();
			}
			ImGui::EndMenu();
		}
		
		if (ImGui::BeginMenu(_T("menu.workspace")))
		{
			for (std::list<CLayoutData *>::iterator it = guiMain->layoutManager->layouts.begin();
				 it != guiMain->layoutManager->layouts.end(); it++)
			{
				CLayoutData *layoutData = *it;
				
				bool isSelected = (layoutData == guiMain->layoutManager->currentLayout);

				char *buf = SYS_GetCharBuf();

				// color on
				if (layoutData->doNotUpdateViewsPositions)
				{
					ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 32.0f));

					sprintf(buf, "*%s##workspacemenu", layoutData->layoutName);
				}
				else
				{
					sprintf(buf, " %s##workspacemenu", layoutData->layoutName);
				}
				
				
				const char *keyShortcutName = layoutData->keyShortcut ? layoutData->keyShortcut->cstr : "";
				if (ImGui::MenuItem(buf, keyShortcutName, &isSelected))
				{
					guiMain->layoutManager->SetLayoutAsync(layoutData, true);
				}
				
				// color off
				if (layoutData->doNotUpdateViewsPositions)
				{
					ImGui::PopStyleVar(1);
					ImGui::PopStyleColor(1);
				}
			}
			
			ImGui::Separator();
			if (ImGui::MenuItem(_T("menu.workspace.new")))
			{
				layoutData = new CLayoutData();
				guiMain->layoutManager->SerializeLayoutAsync(layoutData);
				
				// does not work ImGui::OpenPopup("Store Layout as...");
				openPopupImGuiWorkaround = true;
			}

			if (ImGui::MenuItem(_T("menu.workspace.delete")))
			{
				guiMain->layoutManager->RemoveAndDeleteLayout(guiMain->layoutManager->currentLayout);
				guiMain->layoutManager->StoreLayouts();
			}

			ImGui::EndMenu();
		}

		// -----------------------------------------------------------------
		// Settings menu — app/engine preferences
		// -----------------------------------------------------------------
		if (ImGui::BeginMenu(_T("menu.settings")))
		{
			if (ImGui::BeginMenu(_T("menu.settings.theme")))
			{
				RenderThemeMenu();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu(_T("menu.settings.renderer")))
			{
				RenderRendererMenu();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu(_T("menu.settings.gui_scale")))
			{
				RenderGuiScaleMenu();
				ImGui::EndMenu();
			}

			ImGui::EndMenu();
		}

		// -----------------------------------------------------------------
		// Examples menu — lazy-created engine helper views
		// -----------------------------------------------------------------
		if (ImGui::BeginMenu(_T("menu.examples")))
		{
			if (ImGui::MenuItem(_T("menu.examples.music_player")))
			{
				viewMain->OpenExampleMusicPlayer();
			}

			// ---------------------------------------------------------
			// AI submenu -- gated on MT_CAP_LLM.
			//
			// GREYED OUT, not hidden. A capability the manifest turns off
			// should stay VISIBLE and unavailable: a user who wonders where
			// the LLM features went gets an answer, and a menu that silently
			// changes shape between builds is harder to reason about than one
			// that says why.
			//
			// Two APIs from MT_Capabilities.h, doing two different jobs:
			//
			//   MT_HAS_CAP(MT_CAP_LLM) -- a RUNTIME query, used here as the
			//     enable flag. It folds to a constant, so this costs nothing,
			//     and the menu code stays identical in both builds.
			//
			//   #if MT_CAP_LLM -- COMPILE-TIME removal of the body, which is
			//     required rather than tidy: OpenExampleLlm*() do not exist
			//     when the capability is off.
			// ---------------------------------------------------------
			const bool hasLlm = MT_HAS_CAP(MT_CAP_LLM);
			if (ImGui::BeginMenu(_T("menu.examples.ai"), hasLlm))
			{
#if MT_CAP_LLM
				if (ImGui::MenuItem(_T("menu.examples.ai.llm_settings")))
				{
					viewMain->OpenExampleLlmSettings();
				}
				if (ImGui::MenuItem(_T("menu.examples.ai.llm_chat")))
				{
					viewMain->OpenExampleLlmChat();
				}
				if (ImGui::MenuItem(_T("menu.examples.ai.llm_tasks")))
				{
					viewMain->OpenExampleLlmTasks();
				}
#endif
				ImGui::EndMenu();
			}
			if (!hasLlm && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("%s", _T("menu.examples.ai.unavailable"));
			}

			if (ImGui::MenuItem(_T("menu.examples.image_loader")))
			{
				viewMain->OpenExampleImageLoader();
			}

			if (ImGui::MenuItem(_T("menu.examples.camera")))
			{
				viewMain->OpenExampleCamera();
			}

			// ---------------------------------------------------------
			// Video player -- gated on MT_CAP_VIDEO_PLAYBACK, using the same
			// two APIs and the same greyed-out-not-hidden rule as the AI
			// submenu above. MT_HAS_CAP() for the enable flag, #if for the
			// body, because OpenExampleVideoPlayer() does not exist when the
			// capability is off.
			//
			// This example is also where the engine's CODEC-AVAILABILITY story
			// is demonstrated. Whether the capability is compiled in is a
			// BUILD question and answered here; whether this machine can
			// actually decode a given codec is a RUNTIME question, and the
			// view answers that itself rather than by greying this entry --
			// the same split the HDR bench makes below.
			// ---------------------------------------------------------
			const bool hasVideo = MT_HAS_CAP(MT_CAP_VIDEO_PLAYBACK);
			if (ImGui::MenuItem(_T("menu.examples.video_player"), NULL, false, hasVideo))
			{
#if MT_CAP_VIDEO_PLAYBACK
				viewMain->OpenExampleVideoPlayer();
#endif
			}
			if (!hasVideo && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("%s", _T("menu.examples.video_player.unavailable"));
			}

			// HDR is a CORE engine feature -- it has no MT_CAP_* key, so unlike
			// the AI submenu above there is nothing to gate this on at build
			// time. Whether HDR actually WORKS is a runtime question about the
			// backend and the display, and the view answers it in its own
			// status banner rather than by greying its own menu entry out.
			if (ImGui::MenuItem(_T("menu.examples.hdr_test")))
			{
				viewMain->OpenExampleHdrTest();
			}

			if (ImGui::MenuItem(_T("menu.examples.i18n")))
			{
				viewMain->OpenExampleI18n();
			}

			// ---------------------------------------------------------
			// Undo & Redo -- CUndoManager + ImGuiUndo are unconditionally
			// compiled in the engine (no MT_CAP_* guard wires to them yet),
			// so this entry is ungated like HDR Test / I18N above.
			// ---------------------------------------------------------
			if (ImGui::MenuItem(_T("menu.examples.undo_redo")))
			{
				viewMain->OpenExampleUndoRedo();
			}

			// Gamepad Viewer -- GAM_GamePads is unconditionally compiled
			// (SDL3-based), so this is ungated like the Undo/Redo entry above.
			if (ImGui::MenuItem(_T("menu.examples.gamepad_viewer")))
			{
				viewMain->OpenExampleGamepadViewer();
			}

			// Terminal -- CGuiViewTerminal (libtmt) is also unconditionally
			// compiled, ungated like the two entries above.
			if (ImGui::MenuItem(_T("menu.examples.terminal")))
			{
				viewMain->OpenExampleTerminal();
			}

			// Crash Reporter -- spawns a real child process and shows a native
			// OS dialog, so unlike the entries above this is deliberately
			// excluded from the automated "open views" UI test (same reasoning
			// as Camera's OS permission dialog and Image Loader's native file
			// dialog: presence-only, never auto-clicked).
			if (ImGui::MenuItem(_T("menu.examples.crash_reporter")))
			{
				viewMain->OpenExampleCrashReporter();
			}

			// File Downloader -- CFileDownloader is unconditionally compiled,
			// ungated like the entries above. Opening the view starts nothing
			// by itself; the local server and download only start when the
			// view's own "Start Download" button is pressed.
			if (ImGui::MenuItem(_T("menu.examples.file_downloader")))
			{
				viewMain->OpenExampleFileDownloader();
			}

			ImGui::EndMenu();
		}

		// -----------------------------------------------------------------
		// Help menu
		// -----------------------------------------------------------------
		static bool show_about = false;
		static bool show_app_metrics = false;
		static bool show_app_style_editor = false;
		static bool show_app_demo = false;
		static bool show_app_about = false;

		if (ImGui::BeginMenu(_T("menu.help")))
		{
			ImGui::MenuItem(_T("menu.help.about"), "", &show_about);
			ImGui::Separator();
			ImGui::MenuItem("ImGui Metrics",      "", &show_app_metrics);
			ImGui::MenuItem("ImGui Style Editor", "", &show_app_style_editor);
			ImGui::MenuItem("ImGui Demo",         "", &show_app_demo);
			ImGui::MenuItem("ImGui About",        "", &show_app_about);
			ImGui::Separator();

			// MTEngineSDL internal UI debug / test panel — mirrors c64d
			if (guiMain->viewUiDebug)
			{
				if (ImGui::MenuItem(_T("menu.help.mtenginesdl_test"), "",
				                    &(guiMain->viewUiDebug->visible)))
				{
					guiMain->AddViewSkippingLayout(guiMain->viewUiDebug);
					guiMain->viewUiDebug->SetFocus();
				}
			}

			// end Help menu
			ImGui::EndMenu();
		}
		
		if (show_about)
		{
			ImGui::Begin("DummyApp About", &show_about);
			ImGui::Text("(C) 2021 Marcin Skoczylas, see README for libraries copyright.");
			ImGui::End();
		}
		if (show_app_metrics)       { ImGui::ShowMetricsWindow(&show_app_metrics); }
		if (show_app_style_editor)
		{
			ImGui::Begin("Dear ImGui Style Editor", &show_app_style_editor);
			ImGui::ShowStyleEditor();
			ImGui::End();
		}
		if (show_app_demo)         { ImGui::ShowDemoWindow(&show_app_demo); }
		if (show_app_about)         { ImGui::ShowAboutWindow(&show_app_about); }

		// -----------------------------------------------------------------
		// Language menu
		// -----------------------------------------------------------------
		if (ImGui::BeginMenu(_T("menu.language")))
		{
			CI18nManager *i18n = CI18nManager::Instance();
			const auto &locales = i18n->GetRegisteredLocales();
			for (const auto &locale : locales)
			{
				bool isActive = (locale.tag == i18n->GetActiveLocale());
				if (ImGui::MenuItem(locale.displayName.c_str(), NULL, isActive))
				{
					i18n->SetActiveLocale(locale.tag);
					gApplicationDefaultConfig->SetString("locale", locale.tag.c_str());
				}
			}
			ImGui::EndMenu();
		}

		// done menus
		
		ImGui::EndMainMenuBar();
	}

//	LOGD("layout=%x", layout);
	if (layoutData && openPopupImGuiWorkaround)
	{
		layoutName[0] = 0x00;
		doNotUpdateViewsPosition = false;
		ImGui::OpenPopup("New Workspace");
	}
	
	if (ImGui::BeginPopupModal("New Workspace", NULL, waitingForNewLayoutKeyShortcut ? ImGuiWindowFlags_NoResize : ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (openPopupImGuiWorkaround)
			ImGui::SetKeyboardFocusHere();

		if (waitingForNewLayoutKeyShortcut == false)
		{
			bool saveLayout = false;
			if (ImGui::InputText("Name##NewWorkspacePopup", layoutName, 127, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				saveLayout = true;
			}
			
			ImGui::Checkbox("Do not update views positions", &doNotUpdateViewsPosition);

			if (layoutData->keyShortcut != NULL)
			{
				ImGui::Text("Key shortcut: %s", layoutData->keyShortcut->cstr);
			}

			if (ImGui::Button("Assign keyboard shortcut"))
			{
				waitingForNewLayoutKeyShortcut = true;
			}

			if (ImGui::Button("Cancel"))
			{
				delete layoutData;
				layoutData = NULL;
				ImGui::CloseCurrentPopup();
			}
			
			ImGui::SameLine();
			if (ImGui::Button("Create"))
			{
				saveLayout = true;
			}
			
			if (saveLayout)
			{
				if (layoutName[0] != 0x00)
				{
					layoutData->layoutName = STRALLOC(layoutName);
					layoutData->doNotUpdateViewsPositions = doNotUpdateViewsPosition;
					
					if (layoutData->keyShortcut)
					{
						char *buf = SYS_GetCharBuf();
						sprintf(buf, "Workspace %s", layoutName);
						layoutData->keyShortcut->SetName(buf);
						SYS_ReleaseCharBuf(buf);
					}
					
					guiMain->layoutManager->AddLayout(layoutData);
					guiMain->layoutManager->currentLayout = layoutData;
					guiMain->layoutManager->StoreLayouts();
				}
				else
				{
					delete layoutData;
				}
				layoutData = NULL;
				
				ImGui::CloseCurrentPopup();
			}
		}
		else
		{
			// waiting for key shortcut
			
			ImGui::Text("Hover here your cursor");
			ImGui::Text ("and press a new shortcut key");
		}
		
		openPopupImGuiWorkaround = false;

		ImGui::EndPopup();
	}
}

// check if waiting for key shortcut for new layout
bool CMainMenuBar::KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper)
{
	if (waitingForNewLayoutKeyShortcut)
	{
		if (SYS_IsKeyCodeSpecial(keyCode))
			return false;
		
		if (layoutData->keyShortcut)
		{
			guiMain->RemoveKeyboardShortcut(layoutData->keyShortcut);
			delete layoutData->keyShortcut;
			layoutData->keyShortcut = NULL;
		}
		
		CSlrKeyboardShortcut *findShortcut = guiMain->keyboardShortcuts->FindShortcut(MT_KEYBOARD_SHORTCUT_GLOBAL, keyCode, isShift, isAlt, isControl, isSuper);
		if (findShortcut != NULL)
		{
			char *buf = SYS_GetCharBuf();
			sprintf(buf, "Keyboard shortcut %s is already assigned to %s", findShortcut->cstr, findShortcut->name);
			guiMain->ShowMessageBox("Please revise", buf);
			SYS_ReleaseCharBuf(buf);
			waitingForNewLayoutKeyShortcut = false;
			return true;
		}

		// keyboard shortcut name will be updated on save
		layoutData->keyShortcut = new CSlrKeyboardShortcut(MT_KEYBOARD_SHORTCUT_GLOBAL, "", keyCode, isShift, isAlt, isControl, guiMain->isSuperPressed, guiMain->layoutManager);
		
		waitingForNewLayoutKeyShortcut = false;
		return true;
	}
	return false;
}

// Theme picker.
//
// Driven by CMTThemeRegistry::EnumerateEntries rather than a hardcoded list of
// ImGuiStyleType values (which is what c64d still does): the registry returns
// every theme the host registered first, then the engine's built-in styles, and
// it decides for itself whether "Custom" exists by asking
// VID_HasCustomImGuiStyle(). A host that later registers its own themes gets
// them in this menu with no change here.
//
// The DummyApp registers no themes of its own, so today this enumerates exactly
// the built-in styles — the same list, and the same labels, c64d shows under
// UI > Theme style.
void CMainMenuBar::RenderThemeMenu()
{
	CMTThemeRegistry *registry = CMTThemeRegistry::Instance();

	// Reused across frames: EnumerateEntries clears it, and the Entry strings
	// are borrowed pointers into the registry, never owned here.
	static std::vector<CMTThemeRegistry::Entry> entries;
	registry->EnumerateEntries(entries, /* devBuild */ true);

	const char *activeThemeId = registry->HasActiveTheme() ? registry->GetActiveThemeId() : NULL;
	const int currentLegacyStyle = (int)VID_GetDefaultImGuiStyle();

	for (size_t i = 0; i < entries.size(); i++)
	{
		const CMTThemeRegistry::Entry &entry = entries[i];

		// A registered theme re-applies itself over every engine style change,
		// so while one is active the engine style underneath it is not what the
		// user sees — hence the activeThemeId == NULL guard on the legacy rows.
		bool isSelected = entry.isLegacyStyle
			? (activeThemeId == NULL && entry.legacyStyleType == currentLegacyStyle)
			: (activeThemeId != NULL && strcmp(activeThemeId, entry.id) == 0);

		if (ImGui::MenuItem(entry.label, NULL, isSelected))
		{
			if (entry.isLegacyStyle)
			{
				// Clear the active theme FIRST, for the same reason: it would
				// re-apply itself on top of the style we are about to set and
				// the pick would silently not stick.
				if (activeThemeId != NULL)
					registry->ClearActiveTheme();

				// Stores the choice in the app config too, so it survives restart.
				VID_SetDefaultImGuiStyle((ImGuiStyleType)entry.legacyStyleType);
			}
			else
			{
				registry->SetActiveTheme(entry.id, registry->GetActiveMode(),
				                         registry->GetActiveGuiScale());
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Settings > GUI Scale
// ---------------------------------------------------------------------------
//
// WHY THIS EXISTS AT ALL, because "make the UI bigger" sounds like a nicety and
// on Windows it is not: on a HiDPI display this app renders about half the size
// of every other window on screen.
//
// macOS scales for us. The OS hands the app a backing scale factor, SDL reports
// it, and an 18px font is rasterised into 36 physical pixels -- so it MATCHES
// Explorer's equivalent. Windows does not: a DPI-aware process (which SDL makes
// this one) is given real pixels and is expected to do its own scaling. At 200%
// Windows scaling that means Explorer scales itself and we do not, which is
// exactly the "everything is tiny, but only in this app" symptom.
//
// So this is the manual knob for that. `style.FontScaleMain` is ImGui's own
// documented user-facing scale ("May be set by application once, or exposed to
// end-user") and is the right one here: FontScaleDpi is owned by ImGui's DPI
// path, and writing to it from a menu would fight whatever that path decides.
//
// DISCRETE STEPS, from the engine's ladder, NOT a slider -- MT_Theme.h's own
// reasoning: every value can be tested and pixel-checked, and it bounds
// font-atlas growth.
void CMainMenuBar::RenderGuiScaleMenu()
{
	// MT_GetUiScale(), not style.FontScaleMain. The engine owns the scale now
	// and writes FontScaleMain as one of its effects, so reading it back would
	// be asking the output what the input was -- correct today, and wrong the
	// first time anything else touches that field. Already on the ladder, but
	// clamped anyway so the comparison below is symmetric.
	const float current = MT_ThemeClampGuiScale(MT_GetUiScale());

	for (int i = 0; i < MT_kGuiScaleStepCount; i++)
	{
		const float step = MT_kGuiScaleSteps[i];

		// Percentages are numerals, not translatable text, so they carry no
		// locale key -- the MENU's label does, and that one is _T()'d above.
		char label[16];
		snprintf(label, sizeof(label), "%d%%", (int)(step * 100.0f + 0.5f));

		// Compared through the same clamp that produced `current`, so the tick
		// lands on a rung even when the persisted value was hand-edited to
		// something off-ladder.
		const bool isSelected = (MT_ThemeClampGuiScale(step) == current);

		if (ImGui::MenuItem(label, NULL, isSelected))
		{
			ApplyGuiScale(step);
		}
	}
}

void CMainMenuBar::ApplyGuiScale(float scale)
{
	// MT_SetUiScale, not a raw write to style.FontScaleMain, and it clamps to
	// the ladder itself so this does not have to.
	//
	// The old code set FontScaleMain here and then re-applied the active theme
	// to get geometry to follow -- with a comment conceding that with NO active
	// theme (this app's default) "text scales and geometry does not, which is a
	// real limitation rather than an oversight". That limitation is gone: the
	// engine call scales the geometry table as well when no theme owns the
	// style, and re-applies the theme at the new scale when one does.
	//
	// It is also IDEMPOTENT, which is the part a hand-rolled version gets
	// wrong: ImGuiStyle::ScaleAllSizes() multiplies into the CURRENT sizes and
	// ImTruncs every field, so applying it twice squares the scale and the UI
	// grows every time you touch this menu. MT_UiScaleApplyToImGuiStyle keeps
	// its own copy of the pre-scale geometry and restores it before scaling.
	//
	// Takes effect on the NEXT frame, with no restart and nothing to rebuild --
	// unlike the renderer below, which is why that one gets a message box and
	// this one does not.
	MT_SetUiScale(scale);

	// Persisted so the next launch honours the CHOICE rather than re-detecting
	// the display: DummyAppResolveUiScale() reads this key and only falls back
	// to MT_DetectDisplayUiScale() when it is absent. The same key the engine's
	// other host apps persist, so a reader comparing two apps' config files
	// does not have to work out that they mean the same thing.
	float clamped = MT_GetUiScale();
	gApplicationDefaultConfig->SetFloat("ui.guiScale", &clamped);
}

// ---------------------------------------------------------------------------
// Settings > Renderer -- the draw backend
// ---------------------------------------------------------------------------
//
// Three traps here, and the engine's own header names all three because three
// existing UIs walked into them (VID_Main.h, "Render backend selection").
//
//   1. ENUMERATE, never write the list by hand. VID_GetAvailableRenderBackends()
//      answers for THIS build on THIS platform. Writing out the items is how
//      c64d's menu came to offer "Metal" on Windows.
//
//   2. TICK AGAINST VID_GetEffectiveRenderBackendSelection(), not against a raw
//      compare with the persisted string. Two of the three UIs did the latter
//      and had no answer at all when the persisted value named a backend the
//      build does not have -- a settings file carried from another machine left
//      every item unticked while the app plainly ran one of them.
//
//   3. LIFETIME. VID_GetPreferredRenderBackend(), VID_GetPersistedRenderBackend()
//      and VID_GetEffectiveRenderBackendSelection() share ONE per-thread buffer.
//      A second call silently overwrites the first result, so the value is
//      copied into a std::string before anything else is asked.
//
// And the reason for the footer: a backend switch needs a RESTART. Without a
// line saying so, clicking a row moves the tick and changes nothing on screen,
// which reads as a broken menu rather than as a saved preference.
void CMainMenuBar::RenderRendererMenu()
{
	// Trap 3: copy immediately, before any other VID_Get*Backend* call.
	const std::string effective = VID_GetEffectiveRenderBackendSelection();

	// Trap 1: ask, do not list.
	const char *names[8];
	const int count = VID_GetAvailableRenderBackends(names, 8);

	for (int i = 0; i < count; i++)
	{
		const char *name = names[i];

		// The engine owns the human label, so "Direct3D 11" is spelled the same
		// here as in every other host. Note the SELECTION vocabulary ("d3d11")
		// and the DISPLAY vocabulary ("Direct3D 11") are different on purpose --
		// mixing them is what put "OpenGL4" in a menu that ticked "OpenGL".
		const char *label = VID_GetRenderBackendDisplayName(name);

		// Trap 2: compare against the EFFECTIVE selection.
		const bool isSelected = (effective == name);

		// HDR rides on the backend choice: only Metal on macOS and D3D11 on
		// Windows can drive an extended-range surface. Saying so at the point of
		// choice is the difference between "HDR does not work on my Mac" and
		// "I am on OpenGL".
		//
		// Passed as the SHORTCUT argument rather than drawn with SameLine():
		// ImGui right-aligns it in the menu's shortcut column, and -- the part
		// that matters here -- it stays part of this MenuItem instead of
		// becoming a second, separately addressable item beside it.
		const char *hdrTag = VID_IsRenderBackendHdrCapable(name) ? "HDR" : NULL;

		if (ImGui::MenuItem(label, hdrTag, isSelected))
		{
			// Rejects anything this platform cannot run, and logs when it does,
			// so the menu needs no validation of its own -- it can only ever
			// offer names the availability query already approved.
			VID_SetPreferredRenderBackend(name);

			// A backend switch cannot take effect in the running process: the
			// device, the swapchain and every uploaded texture belong to the
			// backend that is already up. The saved choice applies at the NEXT
			// start, and without saying so the menu looks broken -- the tick
			// moves, the picture does not.
			//
			// The TextDisabled line further down already states this, but it is
			// passive and easy to miss at the moment of clicking. This is the
			// engine's own reusable message box (CGuiMain::ShowMessageBox),
			// used the same way VID_SetViewportsEnable() uses it for the same
			// "needs a restart" situation.
			//
			// INFORMATIONAL, with NO restart callback, unlike that precedent --
			// two reasons, and the first is not obvious from the call site:
			//
			//   1. The popup has exactly ONE button. Its `popen` flag is a local
			//      re-initialised every frame, so the window's X does nothing
			//      persistent and Enter maps to the same action. Attaching
			//      CUiMessageBoxCallbackRestartApplication would therefore make
			//      "restart now" the ONLY way to dismiss it -- there would be no
			//      "understood, later".
			//   2. The user asked to change a SETTING, not to restart. Tearing
			//      the process down on the only available button is a bigger
			//      action than the one they took, and in a real host it could
			//      discard unsaved work.
			//
			// So the copy is a statement, not a question, and the restart stays
			// the user's move. The pending-change line below is what reminds
			// them afterwards.
			//
			// Only when the choice actually CHANGES something: re-picking the
			// backend already running, or re-picking a pending choice, is not
			// worth a modal.
			const char *runningNow = VID_GetCurrentRenderBackendSelection();
			if (runningNow != NULL && effective != name && strcmp(name, runningNow) != 0)
			{
				guiMain->ShowMessageBox(_T("menu.settings.renderer.restart_title"),
										_T("menu.settings.renderer.restart_message"));
			}
		}
	}

	ImGui::Separator();

	// The running backend, in the SAME vocabulary as the items above it --
	// which is exactly what VID_GetCurrentRenderBackendSelection() exists for.
	const char *runningSelection = VID_GetCurrentRenderBackendSelection();
	ImGui::TextDisabled("%s: %s", _T("menu.settings.renderer.running"),
						VID_GetRenderBackendDisplayName(runningSelection));

	if (effective != runningSelection)
	{
		ImGui::TextDisabled("%s", _T("menu.settings.renderer.restart"));
	}

	if (count < 2)
	{
		// Visible and honest rather than hidden. A one-row menu that explains
		// itself beats a menu that is simply not there on this platform.
		ImGui::TextDisabled("%s", _T("menu.settings.renderer.only_one"));
	}

	if (VID_IsRenderBackendOverriddenByCommandLine())
	{
		// The saved choice above is still what is shown and written -- the
		// getters this menu uses deliberately ignore the command line. Say that
		// a flag is winning THIS run, or the menu looks like it does nothing.
		ImGui::TextDisabled("%s", _T("menu.settings.renderer.cli_override"));
	}
}

bool CMainMenuBar::ProcessKeyboardShortcut(u32 zone, u8 actionType, CSlrKeyboardShortcut *keyboardShortcut)
{
	if (keyboardShortcut == kbsQuitApplication)
	{
		viewMain->PrepareForQuit();
		SYS_Shutdown();
	}
	
	return false;
}
