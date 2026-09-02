#pragma once

// CDummyAppI18n
// Registers locales (en, pl, it) with CI18nManager and loads the per-locale
// JSON string tables from assets/locale/.  Call Init() once, early in
// MT_PostInit(), before creating any views that use _T() / _TID().

class CDummyAppI18n
{
public:
	static void Init();
};
