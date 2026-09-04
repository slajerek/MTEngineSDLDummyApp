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
class CViewUndoDemo;
class CViewGamepadViewer;
class CViewTerminalDemo;
class CViewFileDownloaderDemo;
class CViewShaderToyDemo;
class CViewShaderToyOutput;
class CViewShaderToyChannels;
class CViewCodeEditorDemo;

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
	void OpenExampleUndoRedo();
	void OpenExampleGamepadViewer();
	void OpenExampleTerminal();
	void OpenExampleCrashReporter();
	void OpenExampleFileDownloader();
	void OpenExampleShaderToy();
	void OpenExampleCodeEditor();

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

	// Undo/Redo example -- an app-owned view, since no CGuiView wraps
	// CUndoManager anywhere in the engine.
	CViewUndoDemo *viewUndoDemo = NULL;

	// Gamepad viewer example -- an app-owned view over the engine's
	// GAM_EnumerateGamepads(), which has no CGuiView of its own either.
	CViewGamepadViewer *viewGamepadViewer = NULL;

	// Terminal example -- app-owned subclass of the engine's CGuiViewTerminal,
	// wired for local echo instead of a real transport.
	CViewTerminalDemo *viewTerminalDemo = NULL;

	// File Downloader example -- app-owned view around CFileDownloader and a
	// local httplib server started only on demand.
	CViewFileDownloaderDemo *viewFileDownloaderDemo = NULL;

	// Shader Toy example -- a live fragment-shader editor over the engine's
	// CreateCustomFragmentShader seam, which every backend implements.
	CViewShaderToyDemo *viewShaderToyDemo = NULL;

	// The shader's OWN window. Separate from the editor so it can be resized
	// and taken fullscreen without shrinking the text.
	CViewShaderToyOutput *viewShaderToyOutput = NULL;
	CViewShaderToyChannels *viewShaderToyChannels = NULL;

	// Code Editor example -- the engine's CGuiViewCodeEditor with the app's
	// font picker in its toolbar. The wrapper's first caller, and the first
	// use of its one extension point.
	CViewCodeEditorDemo *viewCodeEditor = NULL;

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
