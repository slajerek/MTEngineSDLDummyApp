#include "CTestCodeEditorView.h"
#include "CGuiMain.h"
#include "DBG_Log.h"
#include "CViewDummyAppMain.h"
#include "CViewCodeEditorDemo.h"
#include "DummyAppFonts.h"
#include "CGuiFontManager.h"

#include <cstring>
#include <string>

#define ASSERT_TRUE(cond, msg)                                   \
    do {                                                          \
        if (!(cond)) {                                            \
            char buf[256];                                        \
            snprintf(buf, sizeof(buf), "FAIL: %s", msg);         \
            LOGD("CTestCodeEditorView: %s", buf);                \
            TestCompleted(false, buf);                            \
            return;                                               \
        }                                                         \
        StepCompleted(stepNum++, true, msg);                      \
    } while (0)

CTestCodeEditorView::CTestCodeEditorView() {}
CTestCodeEditorView::~CTestCodeEditorView() {}

void CTestCodeEditorView::Run(ITestCallback *callback)
{
    this->callback = callback;
    isRunning = true;
    int stepNum = 1;

    CViewDummyAppMain *viewMain = dynamic_cast<CViewDummyAppMain *>(guiMain->currentView);
    ASSERT_TRUE(viewMain != nullptr, "main view is CViewDummyAppMain");
    ASSERT_TRUE(viewMain->viewCodeEditor != nullptr, "code editor example view is created");
    CViewCodeEditorDemo *view = viewMain->viewCodeEditor;

    // A text round trip is the whole contract of a thin wrapper -- and it is
    // what was broken in the editor this replaces.
    const char *sample = "int main()\n{\n\treturn 0;\n}\n";
    view->SetText(sample);
    ASSERT_TRUE(view->GetText() == std::string(sample), "text round-trips through the wrapper");
    ASSERT_TRUE(view->GetLanguage() != nullptr, "the view has a syntax highlighting language");

    // Forwarded, not swallowed: checked through the editor the wrapper exposes.
    view->SetReadOnly(true);
    ASSERT_TRUE(view->GetEditor().IsReadOnlyEnabled(), "read-only reaches the editor");
    view->SetReadOnly(false);
    ASSERT_TRUE(!view->GetEditor().IsReadOnlyEnabled(), "and can be turned back off");

    // All three fonts exist -- the two the engine embeds and the one the app
    // does. LoadFonts() ran before the first frame; a nullptr here means an
    // embed step silently failed.
    ASSERT_TRUE(gGuiFontManager.fontMono != nullptr, "engine JetBrains Mono is in the atlas");
    ASSERT_TRUE(gDummyAppFonts.fontCourierPrimeCode != nullptr, "app Courier Prime Code is in the atlas");

    // BEFORE anything applies a choice: the wrapper's default must already be
    // a real font. Views are constructed before LoadFonts(), so a wrapper that
    // captured fontMono in its constructor would hold nullptr here -- and a
    // test that only checked AFTER ApplyFontChoice() would never see it.
    ASSERT_TRUE(view->GetFont() != nullptr, "the wrapper's default font resolves after fonts load");

    // The setting round-trips and the view honours it. SetEditorFontChoice
    // writes settings.hjson IMMEDIATELY, so the user's real choice is mutated
    // here; compute the verdicts, RESTORE, then assert -- an ASSERT between
    // mutation and restore would leave 'courier' behind on failure.
    EDummyAppEditorFont before = gDummyAppFonts.GetEditorFontChoice();
    gDummyAppFonts.SetEditorFontChoice(EDITOR_FONT_COURIER);
    bool choicePersisted = (gDummyAppFonts.GetEditorFontChoice() == EDITOR_FONT_COURIER);
    view->ApplyFontChoice();
    bool viewPickedItUp = (view->GetFont() == gDummyAppFonts.fontCourierPrimeCode);
    gDummyAppFonts.SetEditorFontChoice(before);
    view->ApplyFontChoice();
    ASSERT_TRUE(choicePersisted, "font choice persists");
    ASSERT_TRUE(viewPickedItUp, "and the view picked it up");

    LOGD("CTestCodeEditorView: all steps passed");
    TestCompleted(true, "code editor wrapper forwards text, language, read-only and font");
}

void CTestCodeEditorView::Cancel()
{
    isRunning = false;
}
