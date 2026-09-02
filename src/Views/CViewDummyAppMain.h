#ifndef _CViewDummyAppMain_h_
#define _CViewDummyAppMain_h_

#include "CGuiView.h"
#include "SYS_Defs.h"
#include "CMidiInKeyboard.h"
#include "CSystemFileDialogCallback.h"
#include <list>

// The generated capability header. It lives under $MT_OUT/include/, is written
// by tools/mtcaps on every build, and is on the include path via
// $(MT_CAPS_INCLUDE_DIR) -- so this include works identically from
// ./build-macos.sh and from Xcode.
//
// It gives us MT_CAP_LLM as a 0/1 macro for compile-time removal, and
// MT_HAS_CAP() for the runtime query the menu uses. Including it rather than
// relying on the -D the build passes is deliberate: a misspelt capability then
// fails to compile instead of silently evaluating to 0.
#include "MT_Capabilities.h"

#if MT_CAP_LLM
#include "Sci/Llama/CLlamaService.h"
#endif

class CMainMenuBar;
class CViewImage;
class CMidiInKeyboard;
class CSlrImage;
class CSlrString;
class CGuiViewMusicPlaylist;
#if MT_CAP_LLM
class CGuiViewLlamaModelLoader;
class CGuiViewLlamaChat;
#endif
class CGuiViewImageWithLayer;
class CViewCamera;
class CViewHdrTest;
#if MT_CAP_VIDEO_PLAYBACK
class CGuiViewVideoPlayer;
#endif

class CViewDummyAppMain : public CGuiView, public CMidiInKeyboardCallback, public CSystemFileDialogCallback
{
public:
	CViewDummyAppMain(float posX, float posY, float sizeX, float sizeY);
	virtual ~CViewDummyAppMain();
	virtual void RenderImGui() override;

	// Loads a Latin-Extended UI font (so Polish ąćęłńśźż etc. render) and the
	// engine markdown fonts for AI-chat. Call once before the first frame.
	void LoadFonts();
	
	CMainMenuBar *mainMenuBar;
	
	virtual bool DoTap(float x, float y) override;
	virtual bool KeyDown(u32 keyCode, bool isShift, bool isAlt, bool isControl, bool isSuper) override;

	CMidiInKeyboard *midiIn;
	
	virtual void MidiInKeyboardCallbackNoteOn(int channel, int key, int pressure) override;
	virtual void MidiInKeyboardCallbackNoteOff(int channel, int key, int pressure) override;
	virtual void MidiInKeyboardCallbackControlChange(int knobNum, int value) override;
	virtual void MidiInKeyboardCallbackPitchBend(int value) override;

	// ------------------------------------------------------------------
	// Examples menu — show/focus reusable engine helper views
	// ------------------------------------------------------------------
	void OpenExampleMusicPlayer();
#if MT_CAP_LLM
	void OpenExampleLlmSettings();
	void OpenExampleLlmChat();
	void OpenExampleLlmTasks();
#endif
	void OpenExampleImageLoader();
	void OpenExampleCamera();
	void OpenExampleHdrTest();
#if MT_CAP_VIDEO_PLAYBACK
	void OpenExampleVideoPlayer();
#endif
	void OpenExampleI18n();

	// A discreet, always-true-or-always-false status line about what this
	// MACHINE can decode -- not an event, and deliberately not a popup. See the
	// implementation comment for why it is shaped this way.
	void RenderHeifAvailabilityNotice();
	void PrepareForQuit();

	// Reusable MTEngineSDL example views. They are created hidden at startup and
	// menu actions only show/focus them, matching the pattern used by real apps.
	CGuiViewMusicPlaylist *viewMusicPlaylist = NULL;
#if MT_CAP_LLM
	CGuiViewLlamaModelLoader *viewAiSetup = NULL;
	CGuiViewLlamaChat *viewAiChat = NULL;
#endif
	CGuiViewImageWithLayer *viewImageLoader = NULL;
	CViewCamera *viewCamera = NULL;

	// The HDR test bench. Not gated on any MT_CAP_*: HDR is a core engine
	// feature with no capability key, because a key that cannot change the
	// build is configuration without capability (see mtengine.caps).
	CViewHdrTest *viewHdrTest = NULL;

#if MT_CAP_VIDEO_PLAYBACK
	// The engine's reusable video player. Gated on MT_CAP_VIDEO_PLAYBACK the
	// same way the LLM views are gated on MT_CAP_LLM -- the capability decides
	// whether FFmpeg is linked at all, so the member cannot exist when it is
	// off.
	CGuiViewVideoPlayer *viewVideoPlayer = NULL;
#endif

	virtual void SystemDialogFileOpenSelected(CSlrString *path) override;
	virtual void SystemDialogFileOpenCancelled() override;

private:
	void ShowAndFocus(CGuiView *view);

#if MT_CAP_LLM
	CLlamaService llamaService;
#endif
	std::list<CSlrString *> imageExtensions;
	CSlrImage *imageLoaderImage = NULL;
};

#endif
