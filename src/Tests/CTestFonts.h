#pragma once

#include "CTest.h"

// CTestFonts
// Verifies that the default UI font installed by CViewDummyAppMain::LoadFonts()
// actually bakes Polish (Latin Extended-A) glyphs into the ImGui atlas. If the
// app fell back to the built-in ASCII font, or the chosen TTF lacks Polish, the
// glyph lookups return NULL and this test fails.
class CTestFonts : public CTest
{
public:
	CTestFonts();
	virtual ~CTestFonts();

	virtual const char *GetName() override { return "Fonts"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
