#pragma once

#include "CTest.h"

// CTestI18nDummyApp
// Verifies that CDummyAppI18n successfully registers the three locales, loads
// the string tables, and that the CI18nManager fallback chain works correctly.
class CTestI18nDummyApp : public CTest
{
public:
	CTestI18nDummyApp();
	virtual ~CTestI18nDummyApp();

	virtual const char *GetName() override { return "I18nDummyApp"; }
	virtual void Run(ITestCallback *callback) override;
	virtual void Cancel() override;
};
