#ifdef ENABLE_IMGUI_TEST_ENGINE

#include "imgui.h"
#include "imgui_te_engine.h"
#include "imgui_te_context.h"

// RegisterDummyAppTests is declared extern in DummyInit.cpp and called after
// CImGuiTestEngine::Init() to register all UI tests.
void RegisterDummyAppTests(ImGuiTestEngine *engine)
{
    // Test: Verify main menu bar has the expected top-level items
    ImGuiTest *t = IM_REGISTER_TEST(engine, "ui", "main_menu_bar_items");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuCheck("File");
        ctx->MenuCheck("Workspace");
        ctx->MenuCheck("Help");
    };

    // Test: File menu contains Quit item (don't click — would exit the app)
    t = IM_REGISTER_TEST(engine, "ui", "file_menu_quit");
    t->TestFunc = [](ImGuiTestContext *ctx)
    {
        ctx->SetRef("##MainMenuBar");
        ctx->MenuCheck("File/Quit");
    };
}

#endif // ENABLE_IMGUI_TEST_ENGINE
