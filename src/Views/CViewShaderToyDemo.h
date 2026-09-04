#ifndef _CViewShaderToyDemo_h_
#define _CViewShaderToyDemo_h_

#include "CGuiView.h"
#include "SYS_Defs.h"
#include "TextEditor.h"
#include "CSystemFileDialogCallback.h"

#include <atomic>
#include <string>
#include <list>
#include <utility>
#include <vector>

class CRenderShaderCustomFragment;
class CViewShaderToyChannels;

// A live fragment-shader editor, in the style of shadertoy.com: type
// mainImage(), compile it while the app runs, watch it.
//
// THE EDITOR SPEAKS THE RUNNING BACKEND'S LANGUAGE -- GLSL under OpenGL, MSL
// under Metal, HLSL under D3D11. A transpiler would have bought one source for
// all three at the price of two large engine dependencies; showing the same
// effect written three ways costs a header and makes the differences the
// example's subject.
class CViewShaderToyDemo : public CGuiView, public CSystemFileDialogCallback
{
public:
	CViewShaderToyDemo(const char *name, float posX, float posY, float posZ,
					   float sizeX, float sizeY);
	virtual ~CViewShaderToyDemo();
	virtual void RenderImGui() override;

	// ---- TWO COMPILE ENTRY POINTS, ONE PER TEST FRAMEWORK ---------------
	//
	// The two suites run on DIFFERENT THREADS and the rule for touching the
	// shader is opposite in each. Getting it backwards deadlocks one or
	// crashes the other.
	//
	// CompileNow(): SYNCHRONOUS, RENDER THREAD ONLY, asserts VID_IsRenderThread().
	//   For CTestSuite, whose Run() executes inside MT_Render() -- on the render
	//   thread, inside the frame. A test there that posted a request and polled
	//   would be waiting for a RenderImGui() that cannot run until it returns.
	//   Independent of `visible`, so a test need not open the view.
	//
	// RequestCompile(): records a request that RenderImGui() services on the
	//   next frame. For imgui_test_engine, which runs TestFunc on its own
	//   coroutine thread and must NEVER touch the shader -- a GL call from
	//   there does not fail, it crashes (MT_ShaderProbe.h says the same, for
	//   the same reason). Requires the view to be visible, since
	//   CGuiMain::RenderImGui skips hidden views.
	bool CompileNow();
	void RequestCompile();
	bool IsCompilePending();

	bool IsShaderUsable();
	const char *GetLastCompileError();
	void SetEditorText(const char *source);
	void LoadPreset(int presetIndex);

	// -1 once the text stops being a preset -- any edit, or a file loaded from
	// disk. The combo then reads "Custom", because a label still saying
	// "Tunnel" over text the user has rewritten is a lie the user acts on.
	int GetCurrentPreset();

	// The file dialogs write and read the editor's text.
	virtual void SystemDialogFileOpenSelected(CSlrString *path) override;
	virtual void SystemDialogFileSaveSelected(CSlrString *path) override;

	// "GLSL" | "MSL" | "HLSL" for the running backend.
	static const char *ShaderLanguageForCurrentBackend();

	// The four channel slots this view SAMPLES and does not own. Set once by
	// CViewDummyAppMain immediately after both views exist -- the demo reads
	// from the channels view, so the dependency runs that way and only that
	// way. A NULL here is not a state worth supporting: it would show as four
	// black channels, which reads as a shader bug rather than a wiring one.
	void SetChannelsView(CViewShaderToyChannels *view);
	CViewShaderToyChannels *GetChannelsView();

	// Draw the compiled shader into the CURRENT ImGui window, filling `size`.
	// Called by CViewShaderToyOutput, which owns the window it goes in; the
	// shader, its uniforms and its compile state stay here, with the editor
	// that produces them.
	void RenderShaderInto(const ImVec2 &size);

	// The largest difference between any two UVs of the quad the last draw
	// submitted. It is the ONE number that separates a shader which can vary
	// per pixel from one that cannot: with all-equal UVs -- which is what
	// AddRectFilled produces -- every fragment computes the same coordinate
	// and the effect collapses to a flat colour that only animates. 0 until
	// something has been drawn.
	float GetLastDrawUvSpan();

	// True when there is a compiled shader to draw -- the output window shows
	// its own placeholder text otherwise, rather than an empty rectangle.
	bool HasDrawableShader();

	// How many separate diagnostics the panel is showing. Each gets its own
	// read-only input and a rule above it, so this is also the count of
	// independently selectable blocks.
	int GetErrorBlockCount();

	// True when the editor is currently painting error lines. It is also a
	// check on RebaseLineNumbers recognising the RUNNING driver's diagnostic
	// format: markers come from the line numbers it parsed, so none after a
	// failed compile means the format went unrecognised and every reported
	// line number is being shown unrebased.
	bool HasErrorMarkers();

	// The TextEditor::Language the editor colours with, as an opaque pointer
	// so a test can assert it is set without including TextEditor.h. NULL
	// when no language is set -- which is how "highlighting silently did
	// nothing" looks.
	const void *GetEditorLanguage();

private:
	void ServicePendingCompile();

	// Configures the editor ONCE -- language from the running backend, palette,
	// tab size, and seeds it from editorText. Called from BOTH RenderImGui()
	// and CompileNow(), so a headless test that never renders still sees a
	// configured editor: the language is part of the view's state, not of its
	// drawing.
	void ConfigureEditorIfNeeded();
	const char *PresetForCurrentBackend(int presetIndex);

	// Rewrite the driver's line numbers into the editor's coordinates by
	// subtracting the preamble the backend prepended. Four formats, because
	// each driver spells it differently and the one CI runs (Mesa llvmpipe
	// under xvfb) is not the one any desktop here shows.
	// Also collects, per diagnostic it recognised, the REBASED line number and
	// the driver's line of text -- which is what paints the red backgrounds in
	// the editor and fills their tooltips.
	std::string RebaseLineNumbers(const char *driverLog, int preambleLines,
								  std::vector<std::pair<int, std::string> > *outLines);

	// Paint the editor's error lines, or clear them. Data only, no ImGui call,
	// so CompileNow() can do it.
	void ApplyErrorMarkers(const std::vector<std::pair<int, std::string> > &lines);

	std::string PersistPath();
	void LoadPersisted();
	void SavePersisted();

	CRenderShaderCustomFragment *shader = NULL;
	CViewShaderToyChannels *channelsView = NULL;

	// THE EDITOR OWNS THE BUFFER; editorText is a cache with exactly two rules.
	// PUSH: SetEditorText() and LoadPreset() write editor.SetText(). PULL:
	// CompileNow() reads editor.GetText() before compiling. Nothing else
	// touches the pair -- a missing pull compiles stale text, a spurious push
	// wipes what the user typed.
	TextEditor editor;
	bool editorConfigured = false;
	std::string editorText;
	std::string lastError;
	std::atomic<bool> compileRequested{false};
	// 0..kShaderToyPresetCount-1, or kShaderToyPresetCustom for "not a preset
	// any more".
	int currentPreset = 0;

	// Set while LoadPreset/SystemDialogFileOpenSelected are writing into the
	// editor, so the editor's own change callback does not immediately mark
	// the text we just loaded as Custom.
	bool suppressChangeCallback = false;

	std::list<CSlrString *> shaderFileExtensions;
	u64 startTime = 0;
	u64 lastFrameTime = 0;
	int frameCounter = 0;
	float lastDrawUvSpan = 0.0f;
	std::vector<std::pair<int, std::string> > errorLines;

	// The driver's log split into one block per diagnostic, so the panel can
	// draw a rule between them and hand each to its own read-only input --
	// which is what makes the text selectable and Ctrl+C-able. lastError stays
	// the joined form, because that is what GetLastCompileError() promises and
	// what the Copy button puts on the clipboard.
	std::vector<std::string> errorBlocks;
};

#endif
