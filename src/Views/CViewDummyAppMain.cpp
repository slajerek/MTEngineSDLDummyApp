#include "CViewDummyAppMain.h"
#include "CGuiMain.h"
#include "CMainMenuBar.h"
#include "CI18nManager.h"
#include "CGuiFontManager.h"
#include "RES_ResourceManager.h"
#include "SYS_FileSystem.h"
#include "CSlrString.h"
#include "VID_ImageBinding.h"
#include "CGuiViewMusicPlaylist.h"
#if MT_CAP_LLM
#include "CGuiViewLlamaModelLoader.h"
#include "CGuiViewLlamaChat.h"
#include "CViewLlamaTaskQueue.h"
#endif
#include "CGuiViewImageWithLayer.h"
#include "CViewCamera.h"
#include "CViewHdrTest.h"
// For the HEIF availability API used by RenderHeifAvailabilityNotice().
#include "CImageData.h"
// The persisted GUI scale: the config it lives in, and the clamp that snaps a
// hand-edited value back onto the engine's ladder.
#include "SYS_DefaultConfig.h"
#include "MT_Theme.h"
#if MT_CAP_VIDEO_PLAYBACK
#include "CGuiViewVideoPlayer.h"
#endif
#include "CViewUndoDemo.h"
#include "CViewGamepadViewer.h"
#include "CViewTerminalDemo.h"
#include "MT_CrashReporter.h"
#include "CViewFileDownloaderDemo.h"
#include <string>

using namespace ImGui;

CViewDummyAppMain::CViewDummyAppMain(float posX, float posY, float sizeX, float sizeY)
: CGuiView("CViewDummyAppMain", posX, posY, -1, sizeX, sizeY)
{
	mainMenuBar = new CMainMenuBar(this);
	
	imGuiNoWindowPadding = false;
	imGuiNoScrollbar = false;
	
	midiIn = new CMidiInKeyboard(0, this);;

	imageExtensions.push_back(new CSlrString("png"));
	imageExtensions.push_back(new CSlrString("jpg"));
	imageExtensions.push_back(new CSlrString("jpeg"));

	viewMusicPlaylist = new CGuiViewMusicPlaylist("Music Playlist", 120, 120, -1, 860, 620);
	viewMusicPlaylist->visible = false;
	guiMain->AddView(viewMusicPlaylist);

#if MT_CAP_LLM
	// Guarded, not merely hidden. With MT_CAP_LLM=0 these views are not
	// constructed, not registered, and their classes are not even declared --
	// the engine archive they would need is a stub carrying no llama symbols.
	viewAiSetup = new CGuiViewLlamaModelLoader("AI setup", 60, 90, -1, 560, 260, &llamaService);
	viewAiSetup->visible = false;
	guiMain->AddView(viewAiSetup);

	viewAiChat = new CGuiViewLlamaChat("AI Chat", 100, 120, -1, 640, 480, &llamaService);
	viewAiChat->visible = false;
	viewAiChat->genParamsSource = &viewAiSetup->genParams;
	viewAiChat->modelManager = viewAiSetup->GetModelManager();
	guiMain->AddView(viewAiChat);
#endif

	viewImageLoader = new CGuiViewImageWithLayer("Image Loader", 140, 140, 640, 480);
	viewImageLoader->visible = false;
	guiMain->AddView(viewImageLoader);

	viewCamera = new CViewCamera("Camera", 300, 100, -1, 320, 240);
	viewCamera->visible = false;
	guiMain->AddView(viewCamera);

	// Wide by default: the HDR bench is a control column beside a test pattern
	// plus its numbers, and a narrow window forces the pattern down to a size
	// too small to judge -- which is the one thing the view exists to let you do.
	viewHdrTest = new CViewHdrTest("HDR Test", 160, 160, -1, 1180, 760);
	viewHdrTest->visible = false;
	guiMain->AddView(viewHdrTest);

#if MT_CAP_VIDEO_PLAYBACK
	// 16:9 plus room for the transport controls beneath it, so the default
	// window does not letterbox an ordinary video on first open.
	viewVideoPlayer = new CGuiViewVideoPlayer("Video Player", 180, 180, -1, 960, 620);
	viewVideoPlayer->visible = false;
	guiMain->AddView(viewVideoPlayer);
#endif

	viewUndoDemo = new CViewUndoDemo("Undo & Redo", 200, 200, -1, 420, 260);
	viewUndoDemo->visible = false;
	guiMain->AddView(viewUndoDemo);

	viewGamepadViewer = new CViewGamepadViewer("Gamepad Viewer", 220, 220, -1, 520, 360);
	viewGamepadViewer->visible = false;
	guiMain->AddView(viewGamepadViewer);

	viewTerminalDemo = new CViewTerminalDemo("Terminal", 240, 240, -1, 640, 400);
	viewTerminalDemo->visible = false;
	guiMain->AddView(viewTerminalDemo);

	viewFileDownloaderDemo = new CViewFileDownloaderDemo("File Downloader", 260, 260, -1, 460, 220);
	viewFileDownloaderDemo->visible = false;
	guiMain->AddView(viewFileDownloaderDemo);
}

CViewDummyAppMain::~CViewDummyAppMain()
{
	for (CSlrString *extension : imageExtensions)
	{
		delete extension;
	}
	imageExtensions.clear();

	if (imageLoaderImage)
	{
		VID_PostImageDealloc(imageLoaderImage);
		imageLoaderImage = NULL;
	}
}

void CViewDummyAppMain::LoadFonts()
{
	ImGuiIO &io = ImGui::GetIO();

	// Glyph ranges: Latin + Latin Extended-A/B so Polish (ąćęłńśźż) and other
	// European diacritics render. ImGui only bakes 0x0020-0x00FF by default,
	// which excludes Latin Extended-A where most Polish letters live.
	static const ImWchar latin_ranges[] = {
		0x0020, 0x00FF,  // Basic Latin + Latin-1 Supplement
		0x0100, 0x017F,  // Latin Extended-A
		0x0180, 0x024F,  // Latin Extended-B
		0x2013, 0x2014,  // en dash, em dash
		0,
	};

	ImFontConfig config;
	config.PixelSnapH  = true;
	config.OversampleH = 2;
	config.OversampleV = 2;

	// Located via the engine resolver: repo-relative in dev/tests, the .app
	// bundle Resources when shipped (the Copy Resources phase stages assets/).
	std::string fontDir  = RES_ResolveResourceDir("assets/fonts/", "Roboto-Medium.ttf");
	std::string fontPath = fontDir + "Roboto-Medium.ttf";

	// The GUI scale is NOT read here any more. It is resolved once in
	// DummyInit.cpp's DummyAppResolveUiScale(), which runs BEFORE this view is
	// constructed, and applied through MT_SetUiScale() rather than by writing
	// style.FontScaleMain directly.
	//
	// Two things changed with that move and both were the point:
	//
	//   * FIRST RUN NOW AUTO-DETECTS. This read defaulted to 1.0 when
	//     ui.guiScale was absent, which is exactly the first-run case on a
	//     HiDPI display -- and 1.0 there is the "everything looks tiny" bug,
	//     because a DPI-aware process is handed real physical pixels. The
	//     default is now MT_DetectDisplayUiScale(); a value the user has
	//     actually chosen still wins, because GetFloat() only falls back to the
	//     default when the key is absent.
	//   * GEOMETRY FOLLOWS THE TEXT. FontScaleMain scales text only, so padding
	//     and widget heights stayed at 100% and the UI looked wrong in a way
	//     that was hard to name. MT_SetUiScale() also scales the geometry
	//     table, idempotently.
	//
	// The note that used to live here about SetFloatSkipConfigSave() being
	// unnecessary is still true and is now recorded in the engine's
	// docs/hidpi-ui-scaling.md, next to the mis-test that nearly made it fact.

	ImFont *uiFont = io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 18.0f, &config, latin_ranges);
	if (uiFont != NULL)
	{
		io.FontDefault = uiFont;
		LOGM("CViewDummyAppMain::LoadFonts: UI font loaded from '%s'", fontPath.c_str());
	}
	else
	{
		LOGError("CViewDummyAppMain::LoadFonts: failed to load UI font '%s' — Polish glyphs will be missing", fontPath.c_str());
	}

	// Markdown fonts (Inter + JetBrains Mono) for the AI-chat renderer. The
	// engine now bakes Latin Extended ranges so these also render Polish.
	gGuiFontManager.LoadMarkdownFonts(18.0f);
}

void CViewDummyAppMain::RenderImGui()
{
	if (guiMain->IsViewFullScreen() == false)
	{
		mainMenuBar->RenderImGui();
	}

	if (viewMusicPlaylist)
	{
		viewMusicPlaylist->TickPlayback(ImGui::GetIO().DeltaTime);
	}

#if MT_CAP_LLM
	CViewLlamaTaskQueue::Tick();
	CViewLlamaTaskQueue::Render();
#endif

	PreRenderImGui();

	RenderHeifAvailabilityNotice();

	Text("Press ENTER to toggle full screen");

	if (Button("Press me to quit"))
	{
//		SYS_Sleep(10000);
		PrepareForQuit();
		SYS_Shutdown();
	}
	
	
	static bool clicked = false;
	if (Button("CLICK ME"))
	{
		clicked = true;
	}

	//
	if (clicked)
	{
		SameLine();
		Text("CLICKED");
	}
	
	//
	static int count = 0;

	if (count > 200)
	{
		static int a = 0;
		static int b = 0;
		static int c = 0;

		ImGui::InputInt("A", &a);
		ImGui::InputInt("B", &b);
		ImGui::InputInt("C", &c);

		ImGui::Text("Value of A is %d", a);
		ImGui::Text("Value of B is %d", b);
		ImGui::Text("Value of C is %d", c);
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

void CViewDummyAppMain::ShowAndFocus(CGuiView *view)
{
	if (view == NULL)
		return;

	view->SetVisible(true);
	if (view->imGuiWindow != NULL)
	{
		guiMain->SetFocus(view);
	}
	guiMain->StoreLayoutInSettingsAtEndOfThisFrame();
}

void CViewDummyAppMain::OpenExampleMusicPlayer()
{
	ShowAndFocus(viewMusicPlaylist);
}

#if MT_CAP_LLM
void CViewDummyAppMain::OpenExampleLlmSettings()
{
	ShowAndFocus(viewAiSetup);
}

void CViewDummyAppMain::OpenExampleLlmChat()
{
	ShowAndFocus(viewAiChat);
}

void CViewDummyAppMain::OpenExampleLlmTasks()
{
	CViewLlamaTaskQueue::sOpen = true;
}
#endif

void CViewDummyAppMain::OpenExampleImageLoader()
{
	ShowAndFocus(viewImageLoader);

	CSlrString title("Load Image");
	SYS_DialogOpenFile(this, &imageExtensions, NULL, &title);
}

void CViewDummyAppMain::OpenExampleCamera()
{
	ShowAndFocus(viewCamera);
}

void CViewDummyAppMain::OpenExampleHdrTest()
{
	ShowAndFocus(viewHdrTest);
}

#if MT_CAP_VIDEO_PLAYBACK
void CViewDummyAppMain::OpenExampleVideoPlayer()
{
	ShowAndFocus(viewVideoPlayer);
}
#endif

// The reference pattern for surfacing a MISSING SYSTEM CODEC, and the reason
// it looks this unremarkable.
//
// It is a STATUS LINE, not an event. It asks the engine a question about the
// SYSTEM -- CImageData::GetHeifAvailability() -- so it is simply true or false
// for the whole session, and rendering it every frame costs nothing (the
// engine caches a positive answer, and this app is not opening files in a
// loop). Nothing here counts, suppresses, or remembers having been shown,
// because there is nothing to suppress: a fact about the machine does not
// "fire".
//
// WHAT THIS DELIBERATELY IS NOT:
//
//   NOT a popup or a toast. A modal for something the user did not just do is
//   an interruption; and a toast raised from a decode failure would repeat per
//   file, which for a photo browser opening a folder of HEICs means hundreds
//   of times. If a design needs a "show once" flag to stay tolerable, the
//   design is wrong -- ask the system-level question instead, which is exactly
//   what this does.
//
//   NOT shown when the capability simply is not built. HEIF_UNAVAILABLE_NOT_BUILT
//   is something the USER can do nothing about, so telling them about it is
//   noise about our build. Only the state they can actually fix is surfaced,
//   which is also the only state carrying an install URL.
//
//   NOT an advert. The link is offered once, quietly, beside the explanation,
//   and only because the missing package genuinely is the fix. The engine
//   never opens it; it only hands over the URL as data.
void CViewDummyAppMain::RenderHeifAvailabilityNotice()
{
	const char *installUrl = CImageData::GetHeifCodecInstallUrl();
	if (installUrl == NULL)
	{
		// Either HEIF works, or it is missing for a reason the user cannot
		// act on. Both are silence.
		return;
	}

	const char *i18nKey = CImageData::GetHeifAvailabilityI18nKey();
	if (i18nKey == NULL)
		return;

	// Muted, not an error colour: nothing is broken right now: this is a
	// capability note about what this machine cannot open.
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	ImGui::TextUnformatted(_T(i18nKey));
	ImGui::PopStyleColor();

	ImGui::SameLine();
	// TextLinkOpenURL uses ShellExecuteW on Windows, which is what makes an
	// ms-windows-store:// URL open the Store app rather than a browser.
	ImGui::TextLinkOpenURL(_T("heif.install_link"), installUrl);
}

void CViewDummyAppMain::OpenExampleI18n()
{
	// Show which locale is active and what _T() returns for a key
	CI18nManager *i18n = CI18nManager::Instance();
	char buf[256];
	snprintf(buf, sizeof(buf), "Active locale: %s\n%s",
	         i18n->GetActiveLocale().c_str(),
	         i18n->Get("menu.language"));
	guiMain->ShowNotification("I18N Example", buf);
}

void CViewDummyAppMain::OpenExampleUndoRedo()
{
	ShowAndFocus(viewUndoDemo);
}

void CViewDummyAppMain::OpenExampleGamepadViewer()
{
	ShowAndFocus(viewGamepadViewer);
}

void CViewDummyAppMain::OpenExampleTerminal()
{
	ShowAndFocus(viewTerminalDemo);
}

// Demonstrates the engine's crash-report pipeline without actually crashing
// the running process: MT_CrashReporter_WriteTestReport() writes a synthetic
// report (no real fault, signum = 0), and MT_CrashReporter_SpawnHelper()
// launches a child process with --show-crash-report <path>, which shows the
// native OS dialog and exits on its own -- this process keeps running.
void CViewDummyAppMain::OpenExampleCrashReporter()
{
	char reportPath[512];
	if (MT_CrashReporter_WriteTestReport(reportPath, sizeof(reportPath)))
	{
		MT_CrashReporter_SpawnHelper(reportPath);
		guiMain->ShowNotification("Crash Reporter",
			"Synthetic crash report written; a native dialog will open in a new window.");
	}
	else
	{
		guiMain->ShowNotification("Crash Reporter", "Failed to write the synthetic crash report.");
	}
}

void CViewDummyAppMain::OpenExampleFileDownloader()
{
	ShowAndFocus(viewFileDownloaderDemo);
}

void CViewDummyAppMain::PrepareForQuit()
{
	if (viewCamera && viewCamera->visible)
	{
		viewCamera->SetVisible(false);
	}
}

void CViewDummyAppMain::SystemDialogFileOpenSelected(CSlrString *path)
{
	CSlrImage *image = RES_LoadImageFromFileOS(path, true);
	if (image == NULL)
	{
		guiMain->ShowNotification("Image Loader", "Could not load selected image");
		return;
	}

	if (imageLoaderImage)
	{
		VID_PostImageDealloc(imageLoaderImage);
	}

	imageLoaderImage = image;
	viewImageLoader->SetImage(imageLoaderImage);
	ShowAndFocus(viewImageLoader);
}

void CViewDummyAppMain::SystemDialogFileOpenCancelled()
{
}
