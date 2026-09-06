#include "CTestShaderToyDemo.h"
#include "CGuiMain.h"
#include "DBG_Log.h"
#include "CViewDummyAppMain.h"
#include "CViewShaderToyDemo.h"
#include "CViewShaderToyChannels.h"
#include "ShaderToyPresets.h"
#include "VID_Main.h"

#include <cstring>
#include <string>

#define ASSERT_TRUE(cond, msg)                                   \
    do {                                                          \
        if (!(cond)) {                                            \
            char buf[256];                                        \
            snprintf(buf, sizeof(buf), "FAIL: %s", msg);         \
            LOGD("CTestShaderToyDemo: %s", buf);                 \
            TestCompleted(false, buf);                            \
            return;                                               \
        }                                                         \
        StepCompleted(stepNum++, true, msg);                      \
    } while (0)

CTestShaderToyDemo::CTestShaderToyDemo() {}
CTestShaderToyDemo::~CTestShaderToyDemo() {}

void CTestShaderToyDemo::Run(ITestCallback *callback)
{
    this->callback = callback;
    isRunning = true;
    int stepNum = 1;

    CViewDummyAppMain *viewMain = dynamic_cast<CViewDummyAppMain *>(guiMain->currentView);
    ASSERT_TRUE(viewMain != nullptr, "main view is CViewDummyAppMain");
    ASSERT_TRUE(viewMain->viewShaderToyDemo != nullptr, "shader toy example view is created");

    CViewShaderToyDemo *view = viewMain->viewShaderToyDemo;

    // The whole test is synchronous and must run on the render thread; the
    // view's CompileNow() refuses otherwise. Prove the precondition rather
    // than assume it, so a future change to where CTests run fails HERE with
    // a clear message instead of somewhere inside the GL driver.
    ASSERT_TRUE(VID_IsRenderThread(), "CTest runs on the render thread");

    // ------------------------------------------------------------------
    // 1. The shipped preset compiles on whatever backend is running.
    //
    // No early return for any backend. CI runs one of each -- OpenGL on
    // Linux, Metal on macOS, D3D11 on Windows -- and this is what proves the
    // CreateCustomFragmentShader seam is real on all three rather than
    // declared on all three.
    // ------------------------------------------------------------------
    view->LoadPreset(0);
    bool ok = view->CompileNow();
    if (!ok)
    {
        // Put the driver's complaint in the log before failing: "the preset
        // did not compile" without the reason is the least useful failure
        // this test could produce.
        LOGD("CTestShaderToyDemo: compile error was: %s", view->GetLastCompileError());
    }
    ASSERT_TRUE(ok, "the default preset compiles on this backend");
    ASSERT_TRUE(view->IsShaderUsable(), "and the shader reports itself usable");
    ASSERT_TRUE(strlen(view->GetLastCompileError()) == 0, "and no error is reported");

    // Configured lazily on first use -- CompileNow() above did it -- so a
    // headless test sees the same language a rendered view would. NULL here
    // is how "the editor loaded and highlighting silently did nothing" looks,
    // and no other assertion in this file would notice it: they are all about
    // compiling shaders, not colouring them.
    ASSERT_TRUE(view->GetEditorLanguage() != nullptr,
                "the editor has a syntax highlighting language");

    // ------------------------------------------------------------------
    // 2. The failure path.
    //
    // This is the half where the engine has silently reported false success
    // before: CRenderShaderOpenGL4 used to set isCompiled unconditionally,
    // which made every compile-time assertion green regardless.
    // ------------------------------------------------------------------
    view->SetEditorText("this is not a shader at all");
    bool broken = view->CompileNow();
    ASSERT_TRUE(!broken, "a broken shader is reported as broken");
    ASSERT_TRUE(strlen(view->GetLastCompileError()) > 0,
                "and the driver's diagnostics are RETURNED, not merely logged");
    ASSERT_TRUE(view->IsShaderUsable(),
                "and the previous working shader is still bound");

    // The editor paints the failing lines red, which means RebaseLineNumbers
    // recognised THIS driver's diagnostic format -- markers are built from the
    // line numbers it parsed. None here would mean the format went
    // unrecognised and every number shown to the user is unrebased, pointing
    // at a line that is not the one at fault.
    ASSERT_TRUE(view->HasErrorMarkers(),
                "the failing lines are marked in the editor");

    // The panel splits the log into separate diagnostics -- one read-only
    // input each, with a rule between them. Without the split it is one wall
    // of driver output, which is what it was.
    ASSERT_TRUE(view->GetErrorBlockCount() > 0,
                "the error panel has at least one selectable diagnostic block");

    // ------------------------------------------------------------------
    // 3. Recovery. A failed rebuild must not have left the object wedged --
    //    both compile latches exist to stop per-frame retries and a rebuild
    //    that failed to clear them would report the old verdict forever.
    // ------------------------------------------------------------------
    view->LoadPreset(0);
    ASSERT_TRUE(view->CompileNow(), "restoring valid source recovers");
    ASSERT_TRUE(!view->HasErrorMarkers(), "and the red line markers are cleared");
    ASSERT_TRUE(view->GetErrorBlockCount() == 0, "and the error panel is emptied");

    // ------------------------------------------------------------------
    // 4. EVERY preset compiles, in THIS backend's language.
    //
    // Each preset is written out three times, once per language, and only one
    // of those three is ever exercised on a given machine. Compiling just the
    // first one would leave the other variants unverified until somebody
    // happened to pick them -- and a preset that does not compile is a broken
    // example, not a broken shader.
    // ------------------------------------------------------------------
    for (int i = 0; i < kShaderToyPresetCount; i++)
    {
        view->LoadPreset(i);
        bool presetOk = view->CompileNow();
        if (!presetOk)
        {
            LOGD("CTestShaderToyDemo: preset '%s' failed: %s",
                 kShaderToyPresets[i].name, view->GetLastCompileError());
        }
        char msg[128];
        snprintf(msg, sizeof(msg), "preset '%s' compiles on this backend",
                 kShaderToyPresets[i].name);
        ASSERT_TRUE(presetOk, msg);
    }

    // Selecting a preset says so; editing makes it Custom. A combo still
    // reading "Tunnel" over rewritten text is a label the user acts on.
    view->LoadPreset(0);
    ASSERT_TRUE(view->GetCurrentPreset() == 0, "choosing a preset selects it in the combo");
    ASSERT_TRUE(strlen(view->GetLastCompileError()) == 0, "and clears the error");

    // ------------------------------------------------------------------
    // 4b. ALL FOUR diagnostic formats, on every platform.
    //
    // Section 2 only proves the RUNNING driver's format is recognised. The
    // other three are what other machines see, and the fxc one was wrong for
    // a week without any macOS or Linux run noticing: d3dcompiler_47 prints
    // "Shader@0x<address>(41,12-20): error ..." when it is given no source
    // name, and a marker that insisted on "(" at column 0 missed it. These
    // are the literal shapes each compiler emits, with a 10-line preamble.
    // ------------------------------------------------------------------
    {
        struct { const char *name; const char *log; } kFormats[] = {
            { "Apple GL", "ERROR: 0:15: 'foo' : undeclared identifier\n" },
            { "Mesa",     "0:15(12): error: `foo' undeclared\n" },
            { "Metal",    "program_source:15:12: error: use of undeclared identifier 'foo'\n" },
            { "fxc (named)",   "mainImage(15,12-14): error X3004: undeclared identifier 'foo'\n" },
            { "fxc (unnamed)", "Shader@0x000001C6A9F6E0A0(15,12-14): error X3004: undeclared identifier 'foo'\n" },
            { "fxc (old)",     "(15,12): error X3004: undeclared identifier 'foo'\n" },
            // What Windows actually printed on 2026-09-06: the CURRENT
            // DIRECTORY in front of the placeholder. A directory may contain
            // spaces, so the parser cannot lean on the prefix at all.
            { "fxc (cwd with a space)",
              "C:\\Users\\Jan Kowalski\\App\\Shader@0x00000145FCBDD040(15,1-4): error X3000: unrecognized identifier 'this'\n" },
        };
        for (const auto &f : kFormats)
        {
            std::vector<std::pair<int, std::string> > lines;
            std::string rebased = view->RebaseLineNumbers(f.log, 10, &lines);
            bool parsed = lines.size() == 1 && lines[0].first == 5
                          && rebased.find("15") == std::string::npos;
            if (!parsed)
                LOGD("CTestShaderToyDemo: format '%s' gave %zu line(s): %s",
                     f.name, lines.size(), rebased.c_str());
            char msg[128];
            snprintf(msg, sizeof(msg), "the %s diagnostic format rebases 15 -> 5", f.name);
            ASSERT_TRUE(parsed, msg);
        }
        // Prose with a parenthesis is NOT a diagnostic. This is the line
        // the relaxed fxc marker could have started rewriting.
        std::vector<std::pair<int, std::string> > lines;
        view->RebaseLineNumbers("compiled 3 shader(s), 12 warnings (12,3) in 15 ms\n", 10, &lines);
        ASSERT_TRUE(lines.empty(), "a parenthesis inside prose is left alone");
        // Leave no stale blocks behind from the synthetic logs.
        view->RebaseLineNumbers("", 10, NULL);
    }

    // ------------------------------------------------------------------
    // 5. The channels round-trip.
    //
    // The panel owns the images and the demo view only reads bindings, so
    // this is the seam that has to hold: what the panel is told, the shader
    // is handed. It does not need a file -- channel 0 ships bound to ImGui's
    // font atlas, which is a real texture on every backend.
    // ------------------------------------------------------------------
    CViewShaderToyChannels *channels = view->GetChannelsView();
    ASSERT_TRUE(channels != nullptr, "the editor view knows its channels view");

    // THIS TEST RUNS AGAINST THE USER'S REAL SETTINGS, and the channels are
    // persisted, so every slot it touches is captured first and put back at
    // the end. The first version asserted that channel 3 was empty and failed
    // the moment somebody actually used the feature -- and, worse, its "clear
    // channel 0" step had already thrown away an image the user had loaded. A
    // test may not spend the thing it is checking.
    std::string savedPath[kShaderChannelCount];
    EShaderChannelFilter savedFilter[kShaderChannelCount];
    EShaderChannelWrap savedWrap[kShaderChannelCount];
    bool savedFlipY[kShaderChannelCount];
    for (int i = 0; i < kShaderChannelCount; i++)
    {
        savedPath[i] = channels->GetChannelPath(i);
        SShaderChannelBinding b = channels->GetChannelBinding(i);
        savedFilter[i] = b.filter;
        savedWrap[i] = b.wrap;
        savedFlipY[i] = b.flipY;
    }

    // The font atlas is the DEFAULT for channel 0 and the one texture that
    // exists on every backend with nothing to load, so it is what this asserts
    // against rather than whatever the user happens to have in the slot.
    channels->SetChannelFontAtlas(0);
    SShaderChannelBinding ch0 = channels->GetChannelBinding(0);
    ASSERT_TRUE(ch0.texture != nullptr, "the font atlas binds a texture");
    ASSERT_TRUE(ch0.width > 0.0f && ch0.height > 0.0f,
                "and reports a size, which is what iChannelResolution carries");

    channels->SetChannelFilter(0, SHADER_CHANNEL_NEAREST);
    channels->SetChannelWrap(0, SHADER_CHANNEL_CLAMP);
    ch0 = channels->GetChannelBinding(0);
    ASSERT_TRUE(ch0.filter == SHADER_CHANNEL_NEAREST && ch0.wrap == SHADER_CHANNEL_CLAMP,
                "filter and wrap round-trip through the binding");

    // vflip DEFAULTS ON, which is not a detail: ShaderToy's fragCoord counts
    // from the bottom while a texture's v=0 is its top row, so a channel that
    // arrives unflipped samples every image upside down.
    ASSERT_TRUE(savedFlipY[0], "a channel flips Y by default");
    channels->SetChannelFlipY(0, false);
    ASSERT_TRUE(!channels->GetChannelBinding(0).flipY, "and the toggle reaches the binding");

    // An empty slot is not an error state: it returns a binding whose NULL
    // texture every backend renders as black. Cleared here rather than assumed
    // empty, because the user's own settings decide what is in it.
    channels->ClearChannel(3);
    ASSERT_TRUE(channels->GetChannelBinding(3).texture == nullptr,
                "a cleared channel binds nothing");
    ASSERT_TRUE(strlen(channels->GetChannelPath(3)) == 0, "and forgets its path");

    // Out of range asks for the guard rather than the array.
    SShaderChannelBinding bad = channels->GetChannelBinding(kShaderChannelCount);
    ASSERT_TRUE(bad.texture == nullptr, "an out-of-range channel is refused, not indexed");

    // PUT EVERYTHING BACK, before any assertion that could return early.
    for (int i = 0; i < kShaderChannelCount; i++)
    {
        if (savedPath[i].empty())
            channels->ClearChannel(i);
        else if (savedPath[i] == CViewShaderToyChannels::kFontAtlasPath)
            channels->SetChannelFontAtlas(i);
        else if (!channels->SetChannelImage(i, savedPath[i].c_str()))
        {
            // The file went away between launch and now. The app's own startup
            // does exactly this, so the slot ends up in the state the next
            // launch would have given it anyway.
            channels->ClearChannel(i);
        }
        channels->SetChannelFilter(i, savedFilter[i]);
        channels->SetChannelWrap(i, savedWrap[i]);
        channels->SetChannelFlipY(i, savedFlipY[i]);
    }
    ASSERT_TRUE(strcmp(channels->GetChannelPath(0), savedPath[0].c_str()) == 0,
                "and the user's own channel assignment is left as it was found");

    // ------------------------------------------------------------------
    // 6. THE SOURCE-LEVEL LINT, which is the only thing that ever sees the
    //    other two languages.
    //
    //    Section 4 compiles the running backend's variant and nothing else,
    //    so from macOS the HLSL is never checked by anything at all. These
    //    are string assertions over all three variants of every preset: they
    //    cost nothing and they catch the one mistake that is easy to make
    //    while hand-editing eighteen snippets -- which is the mistake that
    //    would otherwise reach the Windows machine silently.
    // ------------------------------------------------------------------
    for (int i = 0; i < kShaderToyPresetCount; i++)
    {
        const char *variants[3] = { kShaderToyPresets[i].glsl,
                                    kShaderToyPresets[i].msl,
                                    kShaderToyPresets[i].hlsl };
        const char *langNames[3] = { "GLSL", "MSL", "HLSL" };
        char msg[192];

        for (int v = 0; v < 3; v++)
        {
            snprintf(msg, sizeof(msg), "preset '%s' has a non-empty %s variant",
                     kShaderToyPresets[i].name, langNames[v]);
            ASSERT_TRUE(variants[v] != nullptr && strlen(variants[v]) > 0, msg);

            snprintf(msg, sizeof(msg), "preset '%s' %s variant opens with MAIN_IMAGE",
                     kShaderToyPresets[i].name, langNames[v]);
            ASSERT_TRUE(strstr(variants[v], "MAIN_IMAGE") != nullptr, msg);
        }

        // The three variants must sample the SAME channels. A texChannel1
        // that exists in the GLSL and not in the HLSL is a preset that shows
        // a different picture per platform -- which no compiler catches,
        // because both halves are valid shaders.
        for (int ch = 0; ch < kShaderChannelCount; ch++)
        {
            char needle[16];
            snprintf(needle, sizeof(needle), "texChannel%d(", ch);
            bool inGlsl = strstr(variants[0], needle) != nullptr;
            for (int v = 1; v < 3; v++)
            {
                bool inThis = strstr(variants[v], needle) != nullptr;
                snprintf(msg, sizeof(msg),
                         "preset '%s': %s uses texChannel%d exactly as the GLSL does",
                         kShaderToyPresets[i].name, langNames[v], ch);
                ASSERT_TRUE(inGlsl == inThis, msg);
            }
        }
    }

    LOGD("CTestShaderToyDemo: all steps passed");
    TestCompleted(true, "shader compiles, fails cleanly and recovers");
}

void CTestShaderToyDemo::Cancel()
{
    isRunning = false;
}
