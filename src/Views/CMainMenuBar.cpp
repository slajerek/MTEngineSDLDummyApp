#include "GUI_Main.h"
#include "CMainMenuBar.h"
#include "CLayoutManager.h"
#include "CSlrKeyboardShortcuts.h"

CMainMenuBar::CMainMenuBar()
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
		if (ImGui::BeginMenu("File"))
		{
//			if (ImGui::MenuItem("Open", kbsOpenFile->cstr))
//			{
//
//			}
			
			ImGui::Separator();
			
			if (ImGui::MenuItem("Quit", kbsQuitApplication->cstr))
			{
				kbsQuitApplication->Run();
			}
			ImGui::EndMenu();
		}
		
		if (ImGui::BeginMenu("Workspace"))
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
//					ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(1.0f, 0.0f, 0.0f, 1.0f)); // Menu bar background color
					ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 32.0f)); // Menu bar padding

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
//					ImGui::PopStyleColor();
					ImGui::PopStyleVar(1);
					ImGui::PopStyleColor(1);
				}
			}
			
			ImGui::Separator();
			if (ImGui::MenuItem("New Workspace..."))
			{
				layoutData = new CLayoutData();
				guiMain->layoutManager->SerializeLayoutAsync(layoutData);
				
				// does not work ImGui::OpenPopup("Store Layout as...");
				openPopupImGuiWorkaround = true;
			}

			if (ImGui::MenuItem("Delete Workspace"))
			{
				guiMain->layoutManager->RemoveLayout(guiMain->layoutManager->currentLayout);
				guiMain->layoutManager->StoreLayouts();
			}

			ImGui::EndMenu();
		}
		
		static bool show_about = false;
		static bool show_app_metrics = false;
		static bool show_app_style_editor = false;
		static bool show_app_demo = false;
		static bool show_app_about = false;

		if (ImGui::BeginMenu("Help"))
		{
			ImGui::MenuItem("CViewDummyApp about", "", &show_about);
			ImGui::Separator();
			ImGui::MenuItem("ImGui Metrics", "", &show_app_metrics);
			ImGui::MenuItem("ImGui Style Editor", "", &show_app_style_editor);
			ImGui::MenuItem("ImGui Demo", "", &show_app_demo);
			ImGui::MenuItem("ImGui About", "", &show_app_about);
			
			// end Help menu
			ImGui::EndMenu();
		}
		
		if (show_about)
		{
			ImGui::Begin("CViewDummyApp About", &show_about);
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

		// done Help Menu
		
		
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

bool CMainMenuBar::ProcessKeyboardShortcut(u32 zone, u8 actionType, CSlrKeyboardShortcut *keyboardShortcut)
{
	if (keyboardShortcut == kbsQuitApplication)
	{
		SYS_Shutdown();
	}
	
	return false;
}
