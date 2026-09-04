#if MT_ENABLE_IMGUI_TEST_ENGINE

#include "imgui.h"
// For ImGuiContext::ErrorCallback -- the fullscreen test below counts ImGui's
// own recoverable-error reports, which is how it sees a style stack imbalance
// in a build where the assert is compiled out.
#include "imgui_internal.h"

// The generated capability header. These UI tests assert BOTH branches of
// MT_CAP_LLM: with it on the AI submenu works, with it off the submenu is still
// THERE and disabled. Never skip silently -- a suite that quietly shrinks when a
// capability is turned off proves nothing about the off path.
#include "MT_Capabilities.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

#include "VID_Main.h"
#include "CRenderShaderCustomFragment.h"
#include "CGuiMain.h"
#include "CViewDummyAppMain.h"
#include "CViewShaderToyDemo.h"
#include "CViewShaderToyChannels.h"
#include "CViewShaderToyOutput.h"
#include "CViewCodeEditorDemo.h"
// For _T(): the renderer test references the message box by its TRANSLATED
// title, so it survives whatever locale an earlier test left active.
#include "CI18nManager.h"

#include <stdio.h>
#include <string>

// SDL3/SDL.h (pulled in via VID_Main.h) drags in <windows.h> on this platform,
// which redefines the legacy Win16 compatibility macro Yield() to nothing --
// undoing imgui_te_context.h's own #undef of it further up the include chain
// and turning every ctx->Yield() below into a syntax error. Same fix, same
// reason, as imgui_te_context.h's own guard.
#ifdef Yield
#undef Yield
#endif

// ---------------------------------------------------------------------------
// Menu-presence helpers
//
// These deliberately avoid MenuCheck(): that is ImGuiTestAction_Check, which
// *activates* the item to drive it into a checked state. On a plain
// (non-checkable) entry it asserts on
//   ((item.StatusFlags & ImGuiItemStatusFlags_Checked) != 0) == checked
// and on "File/Quit" it would actually quit the app in the middle of the suite.
//
// Menu-bar entries do not live directly under the menu-bar window: ImGui
// submits them inside the window's "##MenuBar" child, and each open menu gets
// its own popup window named "###Menu_<depth>". MenuAction() builds exactly
// these paths internally; the helpers below build the same ones so a presence
// check can look an item up without ever activating it.
// ---------------------------------------------------------------------------

static const char *const kMenuBarPath = "//##MainMenuBar/##MenuBar";

// Top-level menu ("File", "Help", ...). Menu-bar entries are submitted every
// frame, so this needs no menu to be open and has no side effects at all.
static bool TopLevelMenuExists(ImGuiTestContext *ctx, const char *name)
{
	char path[256];
	snprintf(path, sizeof(path), "%s/%s", kMenuBarPath, name);
	return ctx->ItemInfo(path, ImGuiTestOpFlags_NoError).ID != 0;
}

// Entry inside a menu. `menuPath` is opened with MenuClick (opening a menu is
// side-effect free, and MenuAction asserts the path exists), then `leaf` is
// looked up in that menu's popup window without being activated. `depth` is the
// popup nesting level: 0 for "File", 1 for a submenu such as "Examples/AI".
static bool MenuEntryExists(ImGuiTestContext *ctx, const char *menuPath, const char *leaf, int depth)
{
	ctx->MenuClick(menuPath);
	ctx->Yield();

	char path[256];
	snprintf(path, sizeof(path), "//###Menu_%02d/%s", depth, leaf);
	return ctx->ItemInfo(path, ImGuiTestOpFlags_NoError).ID != 0;
}

// Click a menu entry that opens one of the reusable engine helper views, then
// wait for its ImGui window to show up. CGuiView windows are created through
// the layout manager, so they are not in the window list on the very next frame.
static bool OpenedViewWindowExists(ImGuiTestContext *ctx, const char *menuPath, const char *windowName)
{
	ctx->MenuClick(menuPath);

	// "//" makes the lookup absolute: without it WindowInfo() resolves the name
	// against the current SetRef() (the menu bar), which never matches.
	char path[256];
	snprintf(path, sizeof(path), "//%s", windowName);

	for (int i = 0; i < 30; i++)
	{
		ctx->Yield();
		if (ctx->WindowInfo(path, ImGuiTestOpFlags_NoError).ID != 0)
			return true;
	}
	return false;
}

// Right-click a point that is inside BOTH the window and the viewport. A
// window may legitimately extend past the viewport -- this app lays its
// windows out for a desktop-sized main window -- and the test engine clamps a
// mouse position to the viewport, so aiming at a window's true centre can put
// the click outside the window entirely.
static void RightClickInside(ImGuiTestContext *ctx, ImGuiWindow *window, ImGuiViewport *viewport)
{
	ImVec2 target;
	target.x = window->Pos.x + ImMin(window->Size.x * 0.5f, viewport->Size.x * 0.4f);
	target.y = window->Pos.y + ImMin(window->Size.y * 0.5f, viewport->Size.y * 0.4f);
	ctx->MouseMoveToPos(target);
	ctx->MouseClick(ImGuiMouseButton_Right);
	ctx->Yield();
	ctx->Yield();
}

// ImGui reports recoverable programmer errors -- a mismatched
// PushStyleVar/PopStyleVar among them -- through IM_ASSERT_USER_ERROR, which
// calls ErrorLog(): that fires the context's ErrorCallback and then asserts
// only if io.ConfigErrorRecoveryEnableAssert is set. Counting the callback is
// therefore the one way to see such an error in a Release run, where the
// assert is compiled out and the mispop is silently clamped instead.
static int gImGuiRecoverableErrorCount = 0;

static void CountImGuiRecoverableError(ImGuiContext *, void *, const char *msg)
{
	gImGuiRecoverableErrorCount++;
	LOGError("imgui recoverable error: %s", msg);
}

// Close every open menu popup so the next check starts from a clean menu bar.
static void CloseOpenMenus(ImGuiTestContext *ctx)
{
	for (int i = 0; i < 4; i++)
	{
		ctx->KeyPress(ImGuiKey_Escape);
		ctx->Yield();
	}
}

// RegisterDummyAppTests is declared extern in DummyInit.cpp and called after
// CImGuiTestEngine::Init() to register all UI tests.
void RegisterDummyAppTests(ImGuiTestEngine *engine)
{
    // Test: Verify main menu bar has the expected top-level items
    ImGuiTest *t = IM_REGISTER_TEST(engine, "ui", "main_menu_bar_items");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ctx->SetRef("##MainMenuBar");
        IM_CHECK(TopLevelMenuExists(ctx, "File"));
        IM_CHECK(TopLevelMenuExists(ctx, "Workspace"));
        IM_CHECK(TopLevelMenuExists(ctx, "Settings"));
        IM_CHECK(TopLevelMenuExists(ctx, "Examples"));
        IM_CHECK(TopLevelMenuExists(ctx, "Help"));
        IM_CHECK(TopLevelMenuExists(ctx, "Language"));
    };

    // Test: File menu contains Quit item (never activated — that would exit the app)
    t = IM_REGISTER_TEST(engine, "ui", "file_menu_quit");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ctx->SetRef("##MainMenuBar");
        IM_CHECK(MenuEntryExists(ctx, "File", "Quit", 0));
        CloseOpenMenus(ctx);
    };

    // Test: Examples menu exposes the engine helper example views
    t = IM_REGISTER_TEST(engine, "ui", "examples_menu_items");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ctx->SetRef("##MainMenuBar");
        IM_CHECK(MenuEntryExists(ctx, "Examples", "Music Player", 0));
        IM_CHECK(MenuEntryExists(ctx, "Examples", "Image Loader", 0));
        IM_CHECK(MenuEntryExists(ctx, "Examples", "Camera", 0));
        IM_CHECK(MenuEntryExists(ctx, "Examples", "HDR Test", 0));
        IM_CHECK(MenuEntryExists(ctx, "Examples", "I18N", 0));
        IM_CHECK(MenuEntryExists(ctx, "Examples", "Undo & Redo", 0));
        IM_CHECK(MenuEntryExists(ctx, "Examples", "Gamepad Viewer", 0));
        IM_CHECK(MenuEntryExists(ctx, "Examples", "Terminal", 0));
        // Crash Reporter: presence only -- see examples_open_engine_views below
        // for why it is never clicked in this suite.
        IM_CHECK(MenuEntryExists(ctx, "Examples", "Crash Reporter", 0));
        IM_CHECK(MenuEntryExists(ctx, "Examples", "File Downloader", 0));
        IM_CHECK(MenuEntryExists(ctx, "Examples", "Shader Toy", 0));
        IM_CHECK(MenuEntryExists(ctx, "Examples", "Code Editor", 0));
#if MT_CAP_LLM
        IM_CHECK(MenuEntryExists(ctx, "Examples/AI", "LLM Settings", 1));
        IM_CHECK(MenuEntryExists(ctx, "Examples/AI", "LLM Chat", 1));
        IM_CHECK(MenuEntryExists(ctx, "Examples/AI", "LLM Tasks", 1));
#else
        // Capability off: the submenu is still PRESENT and must be DISABLED.
        // Asserting presence matters as much as asserting the disabled state --
        // a menu that vanished would also pass a "cannot open it" check, and
        // vanishing is precisely the behaviour this design rejects.
        // Same path convention as MenuEntryExists: open the parent menu, then
        // look the leaf up inside that popup. ImGuiTestOpFlags_NoError because a
        // lookup that finds nothing must be a failed CHECK here, not a hard
        // abort inside ItemInfo.
        ctx->MenuClick("Examples");
        ctx->Yield();
        ImGuiTestItemInfo ai = ctx->ItemInfo("//###Menu_00/AI", ImGuiTestOpFlags_NoError);
        IM_CHECK(ai.ID != 0);
        IM_CHECK((ai.ItemFlags & ImGuiItemFlags_Disabled) != 0);
#endif
        CloseOpenMenus(ctx);
    };

    // Test: safe example actions open the reusable MTEngineSDL helper windows.
    // Camera can request OS permissions, Image Loader opens a native file
    // dialog, and Crash Reporter spawns a child process and a native OS
    // dialog -- all three are covered by menu presence above rather than
    // clicked.
    t = IM_REGISTER_TEST(engine, "ui", "examples_open_engine_views");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ctx->SetRef("##MainMenuBar");

        // CGuiView::SetVisible() is picked up by the layout manager, and the
        // window's ImGui name is "<label>###<stableId>", so it only enters the
        // window list a few frames after the menu click.
        IM_CHECK(OpenedViewWindowExists(ctx, "Examples/Music Player", "Music Playlist"));
        IM_CHECK(OpenedViewWindowExists(ctx, "Examples/Undo & Redo", "Undo & Redo"));
        IM_CHECK(OpenedViewWindowExists(ctx, "Examples/Gamepad Viewer", "Gamepad Viewer"));
        IM_CHECK(OpenedViewWindowExists(ctx, "Examples/Terminal", "Terminal"));
        // Opening this view starts nothing -- the local server/download only
        // begin when "Start Download" is pressed, which this test never does.
        IM_CHECK(OpenedViewWindowExists(ctx, "Examples/File Downloader", "File Downloader"));
        // Opening this one compiles the stock preset on whatever backend is
        // running; shader_toy_compiles_via_ui below checks that it succeeded.
        IM_CHECK(OpenedViewWindowExists(ctx, "Examples/Shader Toy", "Shader Toy"));
        // Opening the example opens BOTH windows: the editor and the shader.
        IM_CHECK(ImGui::FindWindowByName("Shader Toy Output") != NULL);
        IM_CHECK(OpenedViewWindowExists(ctx, "Examples/Code Editor", "Code Editor"));
#if MT_CAP_LLM
        IM_CHECK(OpenedViewWindowExists(ctx, "Examples/AI/LLM Settings", "AI setup"));
        IM_CHECK(OpenedViewWindowExists(ctx, "Examples/AI/LLM Chat", "AI Chat"));
        IM_CHECK(OpenedViewWindowExists(ctx, "Examples/AI/LLM Tasks", "AI Tasks"));
#else
        // The views are not merely hidden with the capability off -- they were
        // never constructed, so there is no window to find. Assert that, rather
        // than asserting nothing.
        IM_CHECK(ImGui::FindWindowByName("AI setup") == NULL);
        IM_CHECK(ImGui::FindWindowByName("AI Chat") == NULL);
#endif
    };

    // Test: Examples > HDR Test opens the bench and its three tabs render.
    //
    // The tab clicks are the point. The window appearing only proves Begin()
    // ran; each tab's body reads a different part of the live render backend --
    // surface flags, headroom, the resident format of an uploaded RGBA16F
    // texture -- and a null-backend or wrong-enum mistake in any of them shows
    // up as a crash or an assert here rather than on somebody's HDR monitor.
    //
    // NOT gated on any capability: HDR is a core engine feature with no
    // MT_CAP_* key. Whether HDR is AVAILABLE is a runtime question, and this
    // test deliberately asserts nothing about the answer -- it runs headless,
    // where the answer is "no", and the view must work either way.
    t = IM_REGISTER_TEST(engine, "ui", "hdr_test_view_tabs");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ctx->SetRef("##MainMenuBar");
        IM_CHECK(OpenedViewWindowExists(ctx, "Examples/HDR Test", "HDR Test"));

        ctx->SetRef("//HDR Test");
        ctx->ItemClick("**/Pattern");
        ctx->Yield();
        ctx->ItemClick("**/Display");
        ctx->Yield();
        ctx->ItemClick("**/Statistics");
        ctx->Yield();

        // Back to the pattern tab, and regenerate through the button so the
        // whole generate -> upload path runs once inside the test.
        ctx->ItemClick("**/Pattern");
        ctx->Yield();
        ctx->ItemClick("**/Regenerate");
        ctx->Yield();
    };

    // Test: Settings > Theme lists the engine's built-in styles.
    //
    // Only the always-present rows are asserted. The menu is driven by
    // CMTThemeRegistry::EnumerateEntries, so its contents grow with any theme a
    // host registers, and "Custom" appears only once a custom style was saved —
    // pinning the full list would make this test fail on a legitimate change.
    t = IM_REGISTER_TEST(engine, "ui", "settings_menu_theme");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ctx->SetRef("##MainMenuBar");
        IM_CHECK(MenuEntryExists(ctx, "Settings", "Theme", 0));
        IM_CHECK(MenuEntryExists(ctx, "Settings/Theme", "Dark", 1));
        IM_CHECK(MenuEntryExists(ctx, "Settings/Theme", "Light", 1));
        IM_CHECK(MenuEntryExists(ctx, "Settings/Theme", "System", 1));
        CloseOpenMenus(ctx);
    };

    // Test: Settings > Renderer lists exactly the backends this build has here.
    //
    // The expected list is BUILT FROM THE SAME QUERY the menu uses rather than
    // spelled out, because the right answer differs per platform and per build:
    // macOS has OpenGL + Metal, a plain Windows build has only OpenGL, one with
    // MT_RENDER_BACKEND_D3D11 has both. A test that hardcoded two rows would be
    // wrong on two of those three and would have to be edited to stay green --
    // which is how a test stops meaning anything.
    t = IM_REGISTER_TEST(engine, "ui", "settings_menu_renderer");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ctx->SetRef("##MainMenuBar");
        IM_CHECK(MenuEntryExists(ctx, "Settings", "Renderer", 0));

        const char *names[8];
        const int count = VID_GetAvailableRenderBackends(names, 8);
        IM_CHECK(count >= 1);

        for (int i = 0; i < count; i++)
            IM_CHECK(MenuEntryExists(ctx, "Settings/Renderer",
                                     VID_GetRenderBackendDisplayName(names[i]), 1));

        // THE INVARIANT THE PICKER RESTS ON, and the bug the engine's own header
        // describes: if the effective selection were ever a name that is not in
        // the enumerated list, the menu would tick NOTHING while the app plainly
        // ran a backend. Asserting it here is what makes the tick trustworthy.
        const std::string effective = VID_GetEffectiveRenderBackendSelection();
        bool effectiveIsListed = false;
        for (int i = 0; i < count; i++)
            if (effective == names[i])
                effectiveIsListed = true;
        IM_CHECK(effectiveIsListed);

        // THE SAME INVARIANT FOR THE PLATFORM DEFAULT, which now names Metal on
        // macOS and Direct3D 11 on a Windows build that has it. A default that
        // named a backend this machine cannot run would not be a wrong tick --
        // it would be a boot failure on a fresh install, since the factory
        // reads it before any window exists.
        const char *defaultBackend = VID_GetDefaultRenderBackend();
        bool defaultIsListed = false;
        for (int i = 0; i < count; i++)
            if (strcmp(defaultBackend, names[i]) == 0)
                defaultIsListed = true;
        IM_CHECK(defaultIsListed);

        CloseOpenMenus(ctx);
    };

    // Test: the HDR preference is DISABLED when the persisted backend cannot
    // carry HDR, and enabled when it can.
    //
    // Both branches asserted, never skipped -- the same shape as the MT_CAP_LLM
    // tests. Which branch runs here depends on the machine's persisted backend,
    // and that is the point: the test states the RULE rather than an outcome, so
    // it is meaningful on a Metal Mac, an OpenGL Mac and a Linux box alike.
    t = IM_REGISTER_TEST(engine, "ui", "hdr_mode_gated_on_backend");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ctx->SetRef("##MainMenuBar");
        IM_CHECK(OpenedViewWindowExists(ctx, "Examples/HDR Test", "HDR Test"));

        ctx->SetRef("//HDR Test");
        ctx->ItemClick("**/Display");
        ctx->Yield();

        // Keyed on the PERSISTED backend, exactly as the view is -- asking about
        // the running one would make this test disagree with the UI in the one
        // case that matters, right after somebody switches backends.
        const std::string persisted = VID_GetPersistedRenderBackend();
        const bool backendCanHdr = VID_IsRenderBackendHdrCapable(persisted.c_str());

        // "auto" is gated ONLY by the backend; "on" carries the extra
        // display-capability gate, so it is not a witness for this rule.
        ImGuiTestItemInfo autoRadio = ctx->ItemInfo("**/auto", ImGuiTestOpFlags_NoError);
        IM_CHECK(autoRadio.ID != 0);
        IM_CHECK(((autoRadio.ItemFlags & ImGuiItemFlags_Disabled) != 0) == !backendCanHdr);
    };

    // Test: picking a backend actually persists it.
    //
    // The presence test above would still pass with an empty MenuItem body, so
    // this one clicks a row and reads the engine back -- the same shape as
    // settings_theme_applies, and it restores the original choice so running the
    // suite never rewrites the user's saved backend.
    //
    // It targets a backend DIFFERENT from the persisted one where the platform
    // has one, so the click has to change something. Where it does not (a plain
    // OpenGL Linux or Windows build) it clicks the only row and asserts that the
    // value is still persisted -- an assertion, not a skip.
    t = IM_REGISTER_TEST(engine, "ui", "settings_renderer_applies");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        // Copied at once: VID_GetPersistedRenderBackend() shares one per-thread
        // buffer with two sibling getters, and this test calls another below.
        const std::string original = VID_GetPersistedRenderBackend();

        const char *names[8];
        const int count = VID_GetAvailableRenderBackends(names, 8);
        IM_CHECK(count >= 1);

        const char *target = names[0];
        for (int i = 0; i < count; i++)
            if (original != names[i]) { target = names[i]; break; }

        ctx->SetRef("##MainMenuBar");
        char path[256];
        snprintf(path, sizeof(path), "Settings/Renderer/%s",
                 VID_GetRenderBackendDisplayName(target));
        ctx->MenuClick(path);
        ctx->Yield();

        const std::string after = VID_GetPersistedRenderBackend();
        IM_CHECK_STR_EQ(after.c_str(), target);

        // The modal only fires on an actual CHANGE. Where the platform has just
        // one backend (a plain OpenGL Linux or Windows build), the loop above
        // clicked the only row, target == original, and nothing changed -- so
        // there is no "applies at next restart" modal to assert, per this
        // function's own header comment. Checking for it unconditionally here
        // used to fail every single-backend build; it has just never been run
        // on one before.
        if (original != target)
        {
            // Picking a DIFFERENT backend now raises the engine's message box
            // saying the change applies at the next start. ASSERT it and
            // DISMISS it -- both halves matter:
            //
            //   asserting, because the modal is the user-visible half of this
            //   feature and a test that ignored it would keep passing if it
            //   silently stopped appearing;
            //
            //   dismissing, because CGuiMain's popup is MODAL and nothing else
            //   closes it. Leaving it up would bleed into every test that runs
            //   after this one -- passing today and mysteriously not tomorrow.
            //
            // Referenced through _T() rather than a literal so this keeps
            // working whatever locale an earlier test left active.
            const std::string modalRef = "//" + std::string(_T("menu.settings.renderer.restart_title"));
            ctx->SetRef(modalRef.c_str());
            IM_CHECK(ctx->ItemInfo("  OK  ", ImGuiTestOpFlags_NoError).ID != 0);
            ctx->ItemClick("  OK  ");
            ctx->Yield();
            ctx->SetRef("##MainMenuBar");

            VID_SetPreferredRenderBackend(original.c_str());
            const std::string restored = VID_GetPersistedRenderBackend();
            IM_CHECK_STR_EQ(restored.c_str(), original.c_str());
        }
    };

    // Test: picking a theme actually applies it.
    //
    // The presence test above would still pass if RenderThemeMenu's MenuItem
    // bodies were empty, so this one clicks a row and reads the engine back.
    // It restores the original style before finishing, so running the suite
    // does not rewrite the user's saved theme.
    t = IM_REGISTER_TEST(engine, "ui", "settings_theme_applies");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        const ImGuiStyleType originalStyle = VID_GetDefaultImGuiStyle();
        const char *target = (originalStyle == IMGUI_STYLE_LIGHT) ? "Dark" : "Light";
        const ImGuiStyleType targetStyle =
            (originalStyle == IMGUI_STYLE_LIGHT) ? IMGUI_STYLE_DARK : IMGUI_STYLE_LIGHT;

        ctx->SetRef("##MainMenuBar");

        char path[256];
        snprintf(path, sizeof(path), "Settings/Theme/%s", target);
        ctx->MenuClick(path);
        ctx->Yield();

        ImGuiStyleType applied = VID_GetDefaultImGuiStyle();

        // Restore before asserting, so a failure cannot leave the app on a
        // theme the user did not choose.
        if (applied != originalStyle)
            VID_SetDefaultImGuiStyle(originalStyle);
        ctx->Yield();

        IM_CHECK(applied == targetStyle);
        IM_CHECK(VID_GetDefaultImGuiStyle() == originalStyle);
    };

    // Test: Help menu exposes the MTEngineSDL UI debug/test view
    t = IM_REGISTER_TEST(engine, "ui", "help_menu_mtenginesdl_test");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ctx->SetRef("##MainMenuBar");
        IM_CHECK(MenuEntryExists(ctx, "Help", "MTEngineSDL test", 0));
        CloseOpenMenus(ctx);
    };

    // Test: Language menu lists the dummy app locales
    t = IM_REGISTER_TEST(engine, "ui", "language_menu_items");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ctx->SetRef("##MainMenuBar");
        IM_CHECK(MenuEntryExists(ctx, "Language", "English", 0));
        IM_CHECK(MenuEntryExists(ctx, "Language", "Polski", 0));
        IM_CHECK(MenuEntryExists(ctx, "Language", "Italiano", 0));
        CloseOpenMenus(ctx);
    };

    // Test: the custom-fragment seam exists on EVERY backend.
    //
    // No early return on any of them. imgui_test_engine has NO Skipped status
    // -- ImGuiTestStatus is Unknown/Success/Queued/Running/Error/Suspended --
    // so a test that bails out on one backend is counted as PASSED there,
    // which is a permanent false green in the headline number.
    //
    // CREATE AND DELETE ONLY. This runs on the test engine's own coroutine
    // thread; SetFragmentSource() would be a GL call from the wrong thread and
    // that does not fail, it crashes. Deleting an object that never compiled
    // must touch no GL object either -- every implementation's destructor
    // guards its handles, and this test is what proves it.
    t = IM_REGISTER_TEST(engine, "ui", "render_backend_custom_fragment_seam");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        CRenderBackend *backend = VID_GetRenderBackend();
        IM_CHECK(backend != NULL);
        CRenderShaderCustomFragment *shader = backend->CreateCustomFragmentShader("SeamProbe");
        IM_CHECK_SILENT(shader != NULL);
        IM_CHECK(!shader->IsUsable());
        delete shader;
    };

    // Test: the Shader Toy example compiles its preset through the UI.
    //
    // REQUEST AND YIELD, NEVER CompileNow(). TestFunc runs on the test engine's
    // own coroutine thread; the GL context belongs to the render thread, and a
    // GL call from here does not fail, it crashes. The view exposes two entry
    // points precisely so each suite can use the right one -- CTestShaderToyDemo
    // takes the synchronous one because IT runs on the render thread.
    t = IM_REGISTER_TEST(engine, "ui", "shader_toy_compiles_via_ui");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        CViewDummyAppMain *viewMain = dynamic_cast<CViewDummyAppMain *>(guiMain->currentView);
        IM_CHECK(viewMain != NULL);
        CViewShaderToyDemo *view = viewMain->viewShaderToyDemo;
        IM_CHECK(view != NULL);

        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Examples/Shader Toy");
        ctx->SetRef("Shader Toy");
        ctx->ItemClick("Compile");

        // Serviced by RenderImGui() on the render thread, so yield until the
        // request clears rather than spinning.
        for (int i = 0; i < 120 && view->IsCompilePending(); i++)
            ctx->Yield();

        IM_CHECK(!view->IsCompilePending());
        IM_CHECK(view->IsShaderUsable());
        IM_CHECK_STR_EQ(view->GetLastCompileError(), "");
    };

    // Test: the engine's code editor view opens and its widget draws.
    t = IM_REGISTER_TEST(engine, "ui", "code_editor_view_renders");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        CViewDummyAppMain *viewMain = dynamic_cast<CViewDummyAppMain *>(guiMain->currentView);
        IM_CHECK(viewMain != NULL && viewMain->viewCodeEditor != NULL);
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Examples/Code Editor");
        ctx->Yield(); ctx->Yield();
        IM_CHECK(viewMain->viewCodeEditor->GetLanguage() != NULL);
        IM_CHECK(viewMain->viewCodeEditor->GetText().size() > 0);
        // The toolbar hook drew the combo; the wrapper alone would not have.
        ctx->SetRef("Code Editor");
        IM_CHECK(ctx->ItemInfo("Font", ImGuiTestOpFlags_NoError).ID != 0);
    };

    // Test: the Shader Toy editor has a language and actually draws.
    //
    // Separate from shader_toy_compiles_via_ui because they fail for different
    // reasons: that one breaks when the shader pipeline breaks, this one when
    // the editor widget fails to draw or loses its language.
    t = IM_REGISTER_TEST(engine, "ui", "shader_toy_editor_renders");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        CViewDummyAppMain *viewMain = dynamic_cast<CViewDummyAppMain *>(guiMain->currentView);
        IM_CHECK(viewMain != NULL && viewMain->viewShaderToyDemo != NULL);
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Examples/Shader Toy");
        ctx->Yield(); ctx->Yield();
        IM_CHECK(viewMain->viewShaderToyDemo->GetEditorLanguage() != NULL);
        // The editor is a CHILD window, and the lookup MUST be the absolute
        // "//Parent/child" form. A single-part path takes WindowInfo's "root"
        // branch and hashes "##src" as a top-level window, which does not
        // exist; only the multi-part form computes the child's mangled id.
        // OpenedViewWindowExists above documents the same rule.
        IM_CHECK(ctx->WindowInfo("//Shader Toy/##src", ImGuiTestOpFlags_NoError).Window != NULL);
    };

    // Test: the shader quad carries INTERPOLATING uv, so the effect can vary
    // per pixel.
    //
    // This is the regression test for a real bug: the first version drew the
    // quad with AddRectFilled, which writes the font atlas' white-pixel uv
    // into all four vertices. Every fragment then computed the same
    // coordinate, and a tunnel shader rendered as one flat colour that merely
    // pulsed with iTime. Nothing else here would notice -- the shader
    // compiles, the window draws, the language is set, and the picture is
    // simply wrong.
    t = IM_REGISTER_TEST(engine, "ui", "shader_toy_quad_has_varying_uv");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        CViewDummyAppMain *viewMain = dynamic_cast<CViewDummyAppMain *>(guiMain->currentView);
        IM_CHECK(viewMain != NULL && viewMain->viewShaderToyDemo != NULL);
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Examples/Shader Toy");
        // The output window has to actually draw a frame before there is
        // anything to measure.
        for (int i = 0; i < 8; i++)
            ctx->Yield();
        IM_CHECK(viewMain->viewShaderToyDemo->HasDrawableShader());
        // Half a unit apart at least: the quad spans uv 0..1, so anything near
        // zero means the vertices collapsed onto one texel again.
        IM_CHECK_GT(viewMain->viewShaderToyDemo->GetLastDrawUvSpan(), 0.5f);
    };

    // Test: a view can go fullscreen without unbalancing the ImGui style stack.
    //
    // REGRESSION TEST for an engine bug this example was the first host to
    // hit. CGuiView::PreRenderImGui pushed two style vars when
    // `guiMain->viewFullScreen == this`, while PostRenderImGui popped two when
    // `guiMain->viewFullScreen != NULL` -- a different question. Going
    // fullscreen hides the other views, so nobody was left to mispop except
    // guiMain->currentView, which CGuiMain::RenderImGui draws
    // unconditionally: CViewDummyAppMain uses the base Pre/Post pair, so from
    // the first fullscreen frame it popped two style vars it had never pushed
    // and ImGui reported "Calling PopStyleVar() too many times!" every frame.
    // Under a debugger that stops the process, which reads as a freeze.
    //
    // c64d has the same menu item and never saw it, because CViewC64 does not
    // call the base pair at all. Nothing in either app's own code was wrong.
    //
    // It drives the context menu the way a user does, which is also the only
    // safe way from a test thread -- see the comment on the mutex below.
    t = IM_REGISTER_TEST(engine, "ui", "view_fullscreen_keeps_style_stack_balanced");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        CViewDummyAppMain *viewMain = dynamic_cast<CViewDummyAppMain *>(guiMain->currentView);
        IM_CHECK(viewMain != NULL && viewMain->viewShaderToyOutput != NULL);
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Examples/Shader Toy");
        ctx->Yield(); ctx->Yield();

        ImGuiContext *g = ImGui::GetCurrentContext();
        ImGuiErrorCallback savedCallback = g->ErrorCallback;
        void *savedUserData = g->ErrorCallbackUserData;
        bool savedAssert = g->IO.ConfigErrorRecoveryEnableAssert;

        gImGuiRecoverableErrorCount = 0;
        g->ErrorCallback = CountImGuiRecoverableError;
        g->ErrorCallbackUserData = NULL;
        // The assert is what a developer sees; this test wants the COUNT, and
        // an assert here would take the whole suite down instead of failing
        // one case.
        g->IO.ConfigErrorRecoveryEnableAssert = false;

        // THROUGH THE MENU, NOT guiMain->SetViewFullScreen -- and that is a
        // rule, not a preference. SetViewFullScreen calls guiMain->LockMutex();
        // this TestFunc runs on the test engine's own coroutine WHILE the
        // render thread is inside the frame holding that mutex, so calling it
        // from here deadlocks the two threads against each other. Measured:
        // the first version of this test hung the suite for its full 120s
        // timeout, one log line after "SetViewFullScreen: ViewEnterFullScreen".
        // The menu item runs on the render thread, which is where the engine's
        // API expects to be called from -- and it is also what the user does.
        // FOCUS IT FIRST. The example opens three windows and the editor takes
        // focus last, so on a small viewport it covers the output window and a
        // click at the output's centre lands in the editor instead -- which is
        // what the first version of this test did, silently.
        // FOCUS IT AND MOVE IT INTO THE VIEWPORT FIRST, both of which the first
        // version of this test skipped and neither of which is optional here.
        // The example lays its three windows out for a desktop-sized window
        // and the editor takes focus last: on the headless viewport the output
        // window sits at x=975, entirely off-screen, so a click at its centre
        // is clamped away and lands nowhere -- the popup simply never opened
        // and the failure said "unable to locate item", which points at the
        // menu rather than at the mouse.
        ImGuiViewport *viewport = ImGui::GetMainViewport();
        ctx->WindowFocus("//Shader Toy Output");
        ctx->WindowMove("//Shader Toy Output", viewport->Pos + ImVec2(10.0f, 10.0f));
        ctx->Yield();
        ImGuiWindow *outputWindow = ctx->WindowInfo("//Shader Toy Output").Window;
        IM_CHECK(outputWindow != NULL);
        RightClickInside(ctx, outputWindow, viewport);
        ctx->ItemClick("//$FOCUSED/Full screen");
        for (int i = 0; i < 20; i++)
            ctx->Yield();
        bool wentFullScreen = guiMain->IsViewFullScreen();

        // Back out the same way. The window fills the viewport now, so its
        // centre is still the right place to right-click.
        outputWindow = ctx->WindowInfo("//Shader Toy Output").Window;
        if (outputWindow != NULL)
        {
            RightClickInside(ctx, outputWindow, viewport);
            ctx->ItemClick("//$FOCUSED/Leave full screen");
        }
        for (int i = 0; i < 20; i++)
            ctx->Yield();
        bool leftFullScreen = !guiMain->IsViewFullScreen();

        // RESTORED BEFORE THE CHECKS. A failing IM_CHECK returns from the
        // TestFunc, and leaving ImGui's error hooks pointing at this test's
        // static would outlive it.
        int errors = gImGuiRecoverableErrorCount;
        g->ErrorCallback = savedCallback;
        g->ErrorCallbackUserData = savedUserData;
        g->IO.ConfigErrorRecoveryEnableAssert = savedAssert;

        IM_CHECK(wentFullScreen);
        IM_CHECK(leftFullScreen);
        IM_CHECK_EQ(errors, 0);
    };

    // Test: the Channels button opens the channel panel, and the panel's four
    // slots reach the shader.
    //
    // The UI half of what CTestShaderToyDemo checks in data: that the button
    // exists, that the window it opens is real, and that channel 0 arrives
    // bound -- which is what makes the Texture preset show something on a
    // first run rather than a black rectangle.
    t = IM_REGISTER_TEST(engine, "ui", "shader_toy_channels_panel");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        CViewDummyAppMain *viewMain = dynamic_cast<CViewDummyAppMain *>(guiMain->currentView);
        IM_CHECK(viewMain != NULL && viewMain->viewShaderToyDemo != NULL);
        ctx->SetRef("##MainMenuBar");
        ctx->MenuClick("Examples/Shader Toy");
        ctx->Yield(); ctx->Yield();

        ctx->SetRef("Shader Toy");
        ctx->ItemClick("Channels");
        ctx->Yield(); ctx->Yield();

        // "//" makes the lookup absolute -- without it the name resolves
        // against the current ref, which is the editor window and never
        // matches. Yield in a loop for the same reason OpenedViewWindowExists
        // does: a view made visible this frame gets its ImGui window on a
        // later one.
        bool opened = false;
        for (int i = 0; i < 30 && !opened; i++)
        {
            ctx->Yield();
            opened = ctx->WindowInfo("//Shader Toy Channels", ImGuiTestOpFlags_NoError).ID != 0;
        }
        IM_CHECK(opened);

        CViewShaderToyChannels *channels = viewMain->viewShaderToyDemo->GetChannelsView();
        IM_CHECK(channels != NULL);
        IM_CHECK(channels->GetChannelBinding(0).texture != NULL);
    };
}

#endif // MT_ENABLE_IMGUI_TEST_ENGINE
