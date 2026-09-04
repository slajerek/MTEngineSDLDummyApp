#include "CViewShaderToyDemo.h"
#include "ShaderToyPresets.h"
#include "CViewShaderToyChannels.h"

#include "VID_Main.h"
#include "SYS_Funct.h"
#include "DBG_Log.h"
#include "CRenderShaderCustomFragment.h"
#include "Core/Render/CRenderBackend.h"
#include "DummyAppFonts.h"
#include "imgui_stdlib.h"
#include "SYS_FileSystem.h"
#include "CSlrString.h"
#include "CGuiMain.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>

// The settings directory, already ending in a separator. Declared the way
// MT_CrashReporter.cpp declares it -- the engine exposes it as a plain global
// rather than through a header.
extern char *gCPathToSettings;

using namespace ImGui;

CViewShaderToyDemo::CViewShaderToyDemo(const char *name, float posX, float posY, float posZ,
									   float sizeX, float sizeY)
: CGuiView(name, posX, posY, posZ, sizeX, sizeY)
{
	imGuiNoWindowPadding = false;
	imGuiNoScrollbar = false;

	shaderFileExtensions.push_back(new CSlrString("glsl"));
	shaderFileExtensions.push_back(new CSlrString("msl"));
	shaderFileExtensions.push_back(new CSlrString("hlsl"));
	shaderFileExtensions.push_back(new CSlrString("txt"));

	// A previously edited shader for THIS language, or the flagship preset.
	LoadPersisted();

	// Queue rather than compile: the constructor runs from MT_PostInit(), and
	// while that is on the render thread today, a queued request costs nothing
	// and does not depend on it staying that way.
	RequestCompile();
}

CViewShaderToyDemo::~CViewShaderToyDemo()
{
	// The shader owns GPU objects, so this must run on the render thread.
	// It does: the view is deleted from ~CViewDummyAppMain during MT_Shutdown(),
	// which the engine calls from the render thread after the loop stops.
	if (shader != NULL)
	{
		delete shader;
		shader = NULL;
	}
}

// ---------------------------------------------------------------------------
// language + presets
// ---------------------------------------------------------------------------

const char *CViewShaderToyDemo::ShaderLanguageForCurrentBackend()
{
	// The RUNNING-name vocabulary -- OpenGL4 / Metal / D3D11 -- not the
	// selection or display one. docs/render-backends.md keeps the three apart
	// deliberately; mixing them is how a menu once titled itself "OpenGL4" over
	// a ticked "OpenGL".
	const char *backend = VID_GetCurrentRenderBackendName();
	if (backend == NULL)
		return "GLSL";
	if (strcmp(backend, "Metal") == 0)
		return "MSL";
	if (strcmp(backend, "D3D11") == 0)
		return "HLSL";
	return "GLSL";
}

const char *CViewShaderToyDemo::PresetForCurrentBackend(int presetIndex)
{
	if (presetIndex < 0 || presetIndex >= kShaderToyPresetCount)
		presetIndex = 0;

	const SShaderToyPreset &preset = kShaderToyPresets[presetIndex];
	const char *lang = ShaderLanguageForCurrentBackend();
	if (strcmp(lang, "MSL") == 0)
		return preset.msl;
	if (strcmp(lang, "HLSL") == 0)
		return preset.hlsl;
	return preset.glsl;
}

void CViewShaderToyDemo::LoadPreset(int presetIndex)
{
	if (presetIndex < 0 || presetIndex >= kShaderToyPresetCount)
		presetIndex = 0;
	SetEditorText(PresetForCurrentBackend(presetIndex));
	// AFTER SetEditorText: that runs the change callback, which would set
	// Custom over the value written here.
	currentPreset = presetIndex;
}

void CViewShaderToyDemo::SetEditorText(const char *source)
{
	editorText = (source != NULL) ? source : "";
	// PUSH. Before the editor is configured, ConfigureEditorIfNeeded() seeds
	// it from editorText, so nothing is lost by the early return.
	if (editorConfigured)
	{
		suppressChangeCallback = true;
		editor.SetText(editorText);
		suppressChangeCallback = false;
	}
}

const void *CViewShaderToyDemo::GetEditorLanguage()
{
	return (const void *)editor.GetLanguage();
}

void CViewShaderToyDemo::ConfigureEditorIfNeeded()
{
	if (editorConfigured)
		return;

	// Decided once: the backend cannot change without a restart. Not in the
	// constructor -- the render backend is not necessarily up when views are
	// created. MSL is C++14 with attributes, so the C++ definition colours it
	// correctly; a dedicated MSL Language could add [[buffer(n)]] and the
	// metal:: namespace later, and nothing is wrong without it.
	const char *lang = ShaderLanguageForCurrentBackend();
	if (strcmp(lang, "MSL") == 0)
		editor.SetLanguage(TextEditor::Language::Cpp());
	else if (strcmp(lang, "HLSL") == 0)
		editor.SetLanguage(TextEditor::Language::Hlsl());
	else
		editor.SetLanguage(TextEditor::Language::Glsl());

	editor.SetPalette(TextEditor::GetDarkPalette());
	editor.SetTabSize(4);
	editor.SetShowLineNumbersEnabled(true);
	editor.SetText(editorText);

	// ANY edit stops the text being a preset. Without this the combo goes on
	// saying "Tunnel" over something the user has rewritten, and the next
	// glance at it is misleading -- which is the whole reason this exists.
	//
	// Installed AFTER the seeding SetText above, and guarded by
	// suppressChangeCallback, so loading a preset does not immediately mark
	// itself Custom.
	editor.SetChangeCallback([this]()
	{
		if (!suppressChangeCallback)
			currentPreset = kShaderToyPresetCustom;
	}, 0);

	editorConfigured = true;
}

int CViewShaderToyDemo::GetCurrentPreset()
{
	return currentPreset;
}

// ---------------------------------------------------------------------------
// persistence
// ---------------------------------------------------------------------------

std::string CViewShaderToyDemo::PersistPath()
{
	// Beside settings.hjson, exactly as VID_GetCustomStyleFilePath() does.
	// gCPathToSettings already ends in a separator.
	//
	// PER LANGUAGE. A GLSL draft handed to the Metal compiler after a backend
	// switch would greet the user with a wall of errors they did not write.
	if (gCPathToSettings == NULL || gCPathToSettings[0] == '\0')
		return std::string();

	std::string path = gCPathToSettings;
	path += "shadertoy-";
	path += ShaderLanguageForCurrentBackend();
	path += ".txt";
	return path;
}

void CViewShaderToyDemo::LoadPersisted()
{
	std::string path = PersistPath();
	if (!path.empty())
	{
		FILE *f = fopen(path.c_str(), "rb");
		if (f != NULL)
		{
			std::string text;
			char buf[4096];
			size_t got = 0;
			while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
				text.append(buf, got);
			fclose(f);

			if (!text.empty())
			{
				editorText = text;
				return;
			}
		}
	}

	LoadPreset(0);
}

void CViewShaderToyDemo::SavePersisted()
{
	std::string path = PersistPath();
	if (path.empty())
		return;

	// fopen("w") does not create directories, but the settings directory
	// already exists by the time any view runs -- the engine wrote
	// settings.hjson into it during init.
	FILE *f = fopen(path.c_str(), "wb");
	if (f == NULL)
	{
		LOGError("CViewShaderToyDemo: failed to open %s for writing", path.c_str());
		return;
	}
	fwrite(editorText.c_str(), 1, editorText.size(), f);
	fclose(f);
}

// ---------------------------------------------------------------------------
// compiling
// ---------------------------------------------------------------------------

void CViewShaderToyDemo::RequestCompile()
{
	compileRequested.store(true, std::memory_order_release);
}

bool CViewShaderToyDemo::IsCompilePending()
{
	return compileRequested.load(std::memory_order_acquire);
}

bool CViewShaderToyDemo::IsShaderUsable()
{
	return shader != NULL && shader->IsUsable();
}

const char *CViewShaderToyDemo::GetLastCompileError()
{
	return lastError.c_str();
}

bool CViewShaderToyDemo::CompileNow()
{
	// NOT A SOFT CHECK. A GL call from any other thread does not fail, it
	// crashes, and the crash lands far from here. imgui_test_engine runs its
	// TestFunc on a coroutine thread, so this is a live hazard rather than a
	// theoretical one.
	if (!VID_IsRenderThread())
	{
		LOGError("CViewShaderToyDemo::CompileNow called off the render thread");
		lastError = "internal error: compile requested from the wrong thread";
		return false;
	}

	ConfigureEditorIfNeeded();
	// PULL. The editor owns the buffer the user typed into. Without this every
	// compile would use whatever was last pushed -- the single likeliest bug
	// in this change.
	editorText = editor.GetText();

	if (shader == NULL)
	{
		CRenderBackend *backend = VID_GetRenderBackend();
		if (backend != NULL)
			shader = backend->CreateCustomFragmentShader("ShaderToyDemo");

		if (shader == NULL)
		{
			lastError = "this render backend offers no custom fragment shader";
			return false;
		}
	}

	if (shader->SetFragmentSource(editorText.c_str()))
	{
		lastError.clear();
		errorLines.clear();
		errorBlocks.clear();
		ApplyErrorMarkers(errorLines);
		SavePersisted();
		return true;
	}

	lastError = RebaseLineNumbers(shader->GetCompileErrorLog(),
								  shader->GetPreambleLineCount(), &errorLines);
	ApplyErrorMarkers(errorLines);
	return false;
}

void CViewShaderToyDemo::ServicePendingCompile()
{
	if (!compileRequested.exchange(false, std::memory_order_acq_rel))
		return;
	CompileNow();
}

// ---------------------------------------------------------------------------
// error line numbers
// ---------------------------------------------------------------------------

std::string CViewShaderToyDemo::RebaseLineNumbers(const char *driverLog, int preambleLines,
												 std::vector<std::pair<int, std::string> > *outLines)
{
	if (outLines != NULL)
		outLines->clear();

	errorBlocks.clear();

	if (driverLog == NULL || driverLog[0] == '\0')
		return std::string();
	if (preambleLines <= 0)
		return std::string(driverLog);

	// FOUR FORMATS, one per compiler, and the one that matters most is the one
	// no desktop here shows: Linux CI runs Mesa llvmpipe under xvfb.
	//
	//   Apple GL     ERROR: 0:41: ...
	//   Mesa         0:41(12): error: ...
	//   Metal        program_source:41:12: error: ...
	//   fxc          (41,12): error X3000: ...
	//
	// Rather than four regexes, walk the text and rewrite the first integer
	// that follows one of the four markers on each line. Anything unrecognised
	// passes through untouched -- showing the driver's raw words is better than
	// mangling them.
	static const char *kMarkers[] = { "ERROR: 0:", "program_source:", "0:", "(" };

	std::string out;
	const char *p = driverLog;

	while (*p != '\0')
	{
		const char *lineStart = p;
		const char *lineEnd = strchr(p, '\n');
		std::string line = (lineEnd != NULL) ? std::string(lineStart, lineEnd)
											 : std::string(lineStart);

		bool rewritten = false;
		for (int m = 0; m < 4 && !rewritten; m++)
		{
			size_t at = line.find(kMarkers[m]);
			// The bare "0:" and "(" markers are only meaningful at the start of
			// a line; anywhere else they are ordinary text.
			if (at == std::string::npos)
				continue;
			if ((m == 2 || m == 3) && at != 0)
				continue;

			size_t numStart = at + strlen(kMarkers[m]);
			size_t numEnd = numStart;
			while (numEnd < line.size() && line[numEnd] >= '0' && line[numEnd] <= '9')
				numEnd++;
			if (numEnd == numStart)
				continue;

			int reported = atoi(line.substr(numStart, numEnd - numStart).c_str());
			int rebased = reported - preambleLines;
			// Clamp: an error inside the preamble itself would otherwise be
			// reported at line 0 or below, which points at nothing.
			if (rebased < 1)
				rebased = 1;

			char numBuf[24];
			snprintf(numBuf, sizeof(numBuf), "%d", rebased);
			line = line.substr(0, numStart) + numBuf + line.substr(numEnd);
			rewritten = true;
			if (outLines != NULL)
				outLines->push_back(std::make_pair(rebased, line));
			// A recognised diagnostic starts a new block. Whatever follows it
			// and is NOT recognised is that diagnostic's continuation -- gcc
			// and Metal both wrap a long message over several lines -- so it
			// belongs in the same block rather than in one of its own.
			errorBlocks.push_back(std::string());
		}

		if (errorBlocks.empty())
			errorBlocks.push_back(std::string());   // anything before the first diagnostic
		if (!errorBlocks.back().empty())
			errorBlocks.back() += "\n";
		errorBlocks.back() += line;

		out += line;
		if (lineEnd == NULL)
			break;
		out += "\n";
		p = lineEnd + 1;
	}

	return out;
}

int CViewShaderToyDemo::GetErrorBlockCount()
{
	return (int)errorBlocks.size();
}

bool CViewShaderToyDemo::HasErrorMarkers()
{
	return editorConfigured && editor.HasMarkers();
}

void CViewShaderToyDemo::ApplyErrorMarkers(const std::vector<std::pair<int, std::string> > &lines)
{
	if (!editorConfigured)
		return;

	editor.ClearMarkers();

	// The editor's marker lines are ZERO-BASED; a compiler counts from one.
	// Off by one here puts the red band on the line above the mistake, which
	// is worse than no band at all.
	for (size_t i = 0; i < lines.size(); i++)
	{
		int line = lines[i].first - 1;
		if (line < 0)
			line = 0;

		// textColor is a filled rect behind the whole line, so it wants alpha
		// -- opaque red would hide the code the user has to fix. The line
		// number gets the solid colour instead, and both carry the driver's
		// own words as their tooltip.
		editor.AddMarker((size_t)line,
						 IM_COL32(255, 80, 80, 255),
						 IM_COL32(200, 40, 40, 80),
						 lines[i].second, lines[i].second);
	}
}

// ---------------------------------------------------------------------------
// save / load
// ---------------------------------------------------------------------------

void CViewShaderToyDemo::SystemDialogFileSaveSelected(CSlrString *path)
{
	if (path == NULL)
		return;

	// Pull first: the editor owns the buffer, and saving the cache would write
	// whatever was last pushed rather than what is on screen.
	if (editorConfigured)
		editorText = editor.GetText();

	char *cPath = path->GetStdASCII();
	FILE *f = fopen(cPath, "wb");
	if (f == NULL)
	{
		LOGError("CViewShaderToyDemo: failed to open %s for writing", cPath);
		guiMain->ShowNotification("Shader Toy", "Could not save the shader");
		delete [] cPath;
		return;
	}
	fwrite(editorText.c_str(), 1, editorText.size(), f);
	fclose(f);
	delete [] cPath;
}

void CViewShaderToyDemo::SystemDialogFileOpenSelected(CSlrString *path)
{
	if (path == NULL)
		return;

	char *cPath = path->GetStdASCII();
	FILE *f = fopen(cPath, "rb");
	if (f == NULL)
	{
		LOGError("CViewShaderToyDemo: failed to open %s for reading", cPath);
		guiMain->ShowNotification("Shader Toy", "Could not open the shader file");
		delete [] cPath;
		return;
	}
	std::string text;
	char buf[4096];
	size_t got = 0;
	while ((got = fread(buf, 1, sizeof(buf), f)) > 0)
		text.append(buf, got);
	fclose(f);
	delete [] cPath;

	SetEditorText(text.c_str());
	// Loaded text is not a preset, whatever it happens to contain.
	currentPreset = kShaderToyPresetCustom;
	RequestCompile();
}

// ---------------------------------------------------------------------------
// rendering
// ---------------------------------------------------------------------------

void CViewShaderToyDemo::RenderImGui()
{
	PreRenderImGui();

	ServicePendingCompile();
	ConfigureEditorIfNeeded();

	Text("Running on: %s", VID_GetCurrentRenderBackendName());
	SameLine();
	TextDisabled("(%s)", ShaderLanguageForCurrentBackend());

	SetNextItemWidth(160.0f);
	const char *presetLabel = (currentPreset == kShaderToyPresetCustom)
							? "Custom" : kShaderToyPresets[currentPreset].name;
	if (BeginCombo("Preset", presetLabel))
	{
		for (int i = 0; i < kShaderToyPresetCount; i++)
		{
			if (Selectable(kShaderToyPresets[i].name, i == currentPreset))
			{
				LoadPreset(i);
				RequestCompile();
			}
		}
		EndCombo();
	}
	SameLine();
	if (Button("Compile"))
		RequestCompile();
	SameLine();
	TextDisabled("or Alt+Enter / F5");
	SameLine();
	gDummyAppFonts.RenderEditorFontCombo("##font");
	SameLine();
	// The channel panel is a window of its own rather than a section of this
	// one: four thumbnails and eight combos would push the editor down by a
	// third of the window, for something adjusted once and then left alone.
	if (Button("Channels") && channelsView != NULL)
	{
		channelsView->SetVisible(true);
		if (channelsView->imGuiWindow != NULL)
			guiMain->SetFocus(channelsView);
	}

	// Save/Load pinned to the RIGHT edge of the toolbar, out of the way of the
	// controls you use while iterating.
	{
		float buttonsWidth = CalcTextSize("Save").x + CalcTextSize("Load").x
						   + GetStyle().FramePadding.x * 4.0f
						   + GetStyle().ItemSpacing.x;
		float rightEdge = GetWindowContentRegionMax().x;
		SameLine();
		SetCursorPosX(rightEdge - buttonsWidth);
		if (Button("Save"))
		{
			if (editorConfigured)
				editorText = editor.GetText();
			CSlrString *title = new CSlrString("Save shader");
			CSlrString *defaultName = new CSlrString("shader");
			SYS_DialogSaveFile(this, &shaderFileExtensions, defaultName, NULL, title);
		}
		SameLine();
		if (Button("Load"))
		{
			CSlrString *title = new CSlrString("Load shader");
			SYS_DialogOpenFile(this, &shaderFileExtensions, NULL, title);
		}
	}

	// ALT+ENTER, because that is what shadertoy.com uses; F5 for anyone
	// arriving from an IDE. NOT Ctrl+Enter: the editor binds that itself, to
	// insertLineBelow(), and Shift+Enter to insertLineAbove(). No Alt or Super
	// chord with Enter and no function key exists in the editor.
	//
	// Default (focused) routing, NOT RouteGlobal. The editor's child is on the
	// parent's focus route, so a shortcut declared here fires while the user
	// types in it; RouteGlobal would ALSO fire when Shader Toy is merely
	// visible and some other window has focus, which is not what F5 means.
	if (Shortcut(ImGuiMod_Alt | ImGuiKey_Enter)
		|| Shortcut(ImGuiMod_Alt | ImGuiKey_KeypadEnter)
		|| Shortcut(ImGuiKey_F5))
	{
		RequestCompile();
	}

	// The compiler's complaint goes UNDER the editor, where a compiler's output
	// belongs -- above it, every fresh error shoved the code down the screen.
	// Reserve its height first so the editor can still fill what is left.
	float footerHeight = 0.0f;
	if (!lastError.empty())
	{
		int textLines = 1;
		for (size_t i = 0; i < lastError.size(); i++)
			if (lastError[i] == '\n') textLines++;
		// Capped: a driver that reports forty errors must not leave the editor
		// two rows tall. The rest is reachable by scrolling the panel.
		if (textLines > 6) textLines = 6;
		footerHeight = GetFrameHeightWithSpacing()                       // the Copy row
					 + GetTextLineHeightWithSpacing() * (float)textLines
					 + GetStyle().ItemSpacing.y * 3.0f;
	}

	// PushFont, not a TextEditor API: the editor reads ImGui::GetFont() when
	// it renders. nullptr from GetEditorFont means "the current font", so push
	// only when there is something to push.
	ImFont *editorFont = gDummyAppFonts.GetEditorFont(gDummyAppFonts.GetEditorFontChoice());
	if (editorFont != nullptr) PushFont(editorFont);
	editor.Render("##src", ImVec2(-1.0f, -footerHeight));
	if (editorFont != nullptr) PopFont();

	if (!lastError.empty())
	{
		PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.18f, 0.06f, 0.06f, 1.0f));
		if (BeginChild("##compileErrors", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
					   ImGuiWindowFlags_HorizontalScrollbar))
		{
			PopStyleColor();

			if (Button("Copy"))
				SetClipboardText(lastError.c_str());
			SameLine();
			TextDisabled("%d diagnostic%s -- the text below can be selected and copied",
						 (int)errorBlocks.size(), errorBlocks.size() == 1 ? "" : "s");

			// READ-ONLY INPUTS, not TextUnformatted, and that is the whole
			// point: ImGui text is not selectable, an input is. Ctrl+C, drag,
			// double-click and Ctrl+A all work in one for free.
			//
			// One per diagnostic, with a rule between them, because a wall of
			// driver output runs together -- which is exactly what it did
			// before this.
			PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
			PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			for (size_t i = 0; i < errorBlocks.size(); i++)
			{
				if (i > 0)
					Separator();

				int blockLines = 1;
				for (size_t c = 0; c < errorBlocks[i].size(); c++)
					if (errorBlocks[i][c] == '\n') blockLines++;

				char id[32];
				snprintf(id, sizeof(id), "##err%d", (int)i);
				// The frame is sized to the text: an input taller than its
				// content would put empty red rows between the diagnostics and
				// undo the separation the rule just gave them.
				InputTextMultiline(id, &errorBlocks[i],
								   ImVec2(-1.0f, GetTextLineHeight() * (float)blockLines
													+ GetStyle().FramePadding.y * 2.0f),
								   ImGuiInputTextFlags_ReadOnly);
			}
			PopStyleColor(2);
		}
		else
		{
			PopStyleColor();
		}
		EndChild();
	}

	PostRenderImGui();
}

// ---------------------------------------------------------------------------
// the shader itself, drawn into somebody else's window
// ---------------------------------------------------------------------------

bool CViewShaderToyDemo::HasDrawableShader()
{
	return shader != NULL && shader->IsUsable();
}

void CViewShaderToyDemo::SetChannelsView(CViewShaderToyChannels *view)
{
	channelsView = view;
}

CViewShaderToyChannels *CViewShaderToyDemo::GetChannelsView()
{
	return channelsView;
}

void CViewShaderToyDemo::RenderShaderInto(const ImVec2 &size)
{
	// The shader stays usable through a failed compile, so a typo leaves the
	// last working effect on screen instead of a black rectangle.
	if (!HasDrawableShader() || size.x <= 1.0f || size.y <= 1.0f)
		return;

	ImDrawList *dl = GetWindowDrawList();
	ImVec2 p0 = GetCursorScreenPos();
	ImVec2 p1 = ImVec2(p0.x + size.x, p0.y + size.y);

	u64 now = SYS_GetCurrentTimeInMillis();
	if (startTime == 0)
	{
		startTime = now;
		lastFrameTime = now;
	}
	ImVec2 mouse = GetMousePos();

	SShaderToyUniforms u = {};
	u.resolution[0] = size.x;
	u.resolution[1] = size.y;
	u.resolution[2] = 1.0f;
	u.time      = (float)(now - startTime) / 1000.0f;
	u.timeDelta = (float)(now - lastFrameTime) / 1000.0f;
	u.frame     = frameCounter++;
	// ShaderToy's iMouse: xy is the position while held, zw the click
	// position. Pixels, origin bottom-left, so Y flips against p1.
	bool down = IsMouseDown(ImGuiMouseButton_Left);
	bool clicked = IsMouseClicked(ImGuiMouseButton_Left);
	u.mouse[0] = down ? mouse.x - p0.x : 0.0f;
	u.mouse[1] = down ? p1.y - mouse.y : 0.0f;
	u.mouse[2] = clicked ? mouse.x - p0.x : 0.0f;
	u.mouse[3] = clicked ? p1.y - mouse.y : 0.0f;
	lastFrameTime = now;

	// THE REST OF THE BLOCK, which nothing else fills. `SShaderToyUniforms u = {}`
	// zero-initialises, so a field left out here does not read as missing in
	// the shader -- it reads as zero, which for iFrameRate and iDate is a
	// plausible-looking lie.
	u.frameRate = GetIO().Framerate;
	// No audio channel exists, and iSampleRate still has to be RIGHT rather
	// than zero: a shader that divides by it should get the number every
	// ShaderToy shader assumes.
	u.sampleRate = 44100.0f;

	// iDate: year, month from 0, day, and seconds into the day.
	// localtime_r does not exist on MSVC and localtime_s takes its two
	// arguments the other way round, so this branches rather than shipping
	// something that builds on two platforms out of three.
	time_t nowTime = time(NULL);
	struct tm localTime = {};
#if defined(_WIN32)
	localtime_s(&localTime, &nowTime);
#else
	localtime_r(&nowTime, &localTime);
#endif
	u.date[0] = (float)(localTime.tm_year + 1900);
	u.date[1] = (float)localTime.tm_mon;
	u.date[2] = (float)localTime.tm_mday;
	u.date[3] = (float)(localTime.tm_hour * 3600 + localTime.tm_min * 60 + localTime.tm_sec);

	// THE CHANNELS, texture and metadata together, all inside one loop -- the
	// binding and the uniforms that describe it have to agree, and they only
	// agree for certain if nothing separates them.
	for (int i = 0; i < kShaderChannelCount; i++)
	{
		SShaderChannelBinding binding;
		if (channelsView != NULL)
			binding = channelsView->GetChannelBinding(i);

		shader->SetChannelTexture(i, binding.texture);
		shader->SetChannelSampler(i, binding.filter, binding.wrap);

		u.channelResolution[i][0] = binding.width;
		u.channelResolution[i][1] = binding.height;
		u.channelResolution[i][2] = 1.0f;
		u.channelUvTransform[i][0] = binding.uvScaleX;
		u.channelUvTransform[i][1] = binding.uvScaleY;
		// .z is vflip: ShaderToy's fragCoord is bottom-left and a texture's
		// v=0 is its top row, so an unflipped channel samples upside down.
		u.channelUvTransform[i][2] = binding.flipY ? 1.0f : 0.0f;
		u.channelWrap[i] = (binding.wrap == SHADER_CHANNEL_REPEAT) ? 1.0f : 0.0f;
		// Playback position within a channel. A still image has none, so 0 is
		// the correct answer and stays correct until a video or audio channel
		// exists.
		u.channelTime[i] = 0.0f;
	}

	shader->SetUniforms(u);

	// AddImage, NOT AddRectFilled -- and this is the whole reason the first
	// version of this example drew one flat pulsing colour instead of a tunnel.
	//
	// AddRectFilled writes the font atlas' WHITE PIXEL uv into all four
	// vertices (imgui_draw.cpp, TexUvWhitePixel), so Frag_UV is CONSTANT over
	// the quad: every pixel computed the same fragCoord and the shader could
	// only vary with iTime. MT_ShaderProbe draws that way legitimately --
	// its shader is a flat colour that never reads uv -- and copying the
	// pattern for a shader that does read it was the mistake.
	//
	// AddImage's default uv_min/uv_max are (0,0)-(1,1), so the four vertices
	// carry interpolating UVs and fragCoord varies per pixel at last. The
	// atlas passed here is NOT what iChannel0 samples -- ImGui binds it at
	// slot 0 for its own draw command, and the channels live at slots 1..4 --
	// so it is only a valid texture ref to hand ImGui with.
	shader->UseShaderProgram();
	int vtxBefore = dl->VtxBuffer.Size;
	dl->AddImage(GetIO().Fonts->TexRef, p0, p1);
	shader->ResetState();

	// Measure what ImGui was actually handed, rather than trusting that the
	// call above does what its documentation says. This is the assertion that
	// would have caught the flat-colour bug on the day it shipped.
	lastDrawUvSpan = 0.0f;
	for (int i = vtxBefore; i < dl->VtxBuffer.Size; i++)
	{
		for (int j = vtxBefore; j < dl->VtxBuffer.Size; j++)
		{
			float du = dl->VtxBuffer[i].uv.x - dl->VtxBuffer[j].uv.x;
			float dv = dl->VtxBuffer[i].uv.y - dl->VtxBuffer[j].uv.y;
			if (du < 0.0f) du = -du;
			if (dv < 0.0f) dv = -dv;
			if (du > lastDrawUvSpan) lastDrawUvSpan = du;
			if (dv > lastDrawUvSpan) lastDrawUvSpan = dv;
		}
	}

	// Claim the space, so a caller can put anything after it.
	Dummy(size);
}

float CViewShaderToyDemo::GetLastDrawUvSpan()
{
	return lastDrawUvSpan;
}
