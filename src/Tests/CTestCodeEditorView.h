#pragma once

#include "CTest.h"

// CTestCodeEditorView
//
// Asserts the engine's CGuiViewCodeEditor wrapper is thin in the way that
// matters: text round-trips through it unchanged (which is what was broken in
// the editor it replaces), read-only reaches the widget rather than stopping
// at the wrapper, a language is set, and its default font resolves to a real
// font AFTER fonts load -- checked BEFORE any explicit choice is applied, so a
// wrapper that captured the font pointer in its constructor (which runs before
// LoadFonts()) would fail here rather than silently fall back to the UI font.
//
// Also asserts all three editor fonts are in the atlas, including the one
// this app embeds, and that the shared editor.font setting round-trips.
class CTestCodeEditorView : public CTest
{
public:
	CTestCodeEditorView();
	virtual ~CTestCodeEditorView();

	virtual const char *GetName() override { return "CodeEditorView"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
