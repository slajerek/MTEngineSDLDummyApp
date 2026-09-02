#include "CDummyAppI18n.h"
#include "CI18nManager.h"
#include "RES_ResourceManager.h"
#include "SYS_DefaultConfig.h"
#include "DBG_Log.h"
#include <string>

// ============================================================================
// Init
// ============================================================================
//
// Plural rules come from the engine's built-in CLDR tables
// (CI18nManager::GetBuiltinCardinalRule / GetBuiltinOrdinalRule); apps no longer
// hand-roll them. Pass nullptr (the default) for a locale with no built-in rule.

void CDummyAppI18n::Init()
{
	CI18nManager *mgr = CI18nManager::Instance();

	// --- Register locales ---

	// English
	SI18nLocale en;
	en.tag         = "en";
	en.displayName = "English";
	en.cardinalRule = CI18nManager::GetBuiltinCardinalRule("en");
	en.ordinalRule  = CI18nManager::GetBuiltinOrdinalRule("en");
	en.numberFormat.decimalSeparator   = ".";
	en.numberFormat.thousandsSeparator = ",";
	en.numberFormat.groupingSize       = 3;
	mgr->RegisterLocale(en);

	// Polish
	SI18nLocale pl;
	pl.tag              = "pl";
	pl.displayName      = "Polski";
	pl.explicitFallbacks = {"en"};
	pl.cardinalRule     = CI18nManager::GetBuiltinCardinalRule("pl");
	pl.ordinalRule      = CI18nManager::GetBuiltinOrdinalRule("pl");
	pl.numberFormat.decimalSeparator   = ",";
	pl.numberFormat.thousandsSeparator = " ";
	pl.numberFormat.groupingSize       = 3;
	mgr->RegisterLocale(pl);

	// Italian
	SI18nLocale it;
	it.tag              = "it";
	it.displayName      = "Italiano";
	it.explicitFallbacks = {"en"};
	it.cardinalRule     = CI18nManager::GetBuiltinCardinalRule("it");
	it.ordinalRule      = CI18nManager::GetBuiltinOrdinalRule("it");
	it.numberFormat.decimalSeparator   = ",";
	it.numberFormat.thousandsSeparator = ".";
	it.numberFormat.groupingSize       = 3;
	mgr->RegisterLocale(it);

	// --- Load string tables ---
	// Engine resolves the locale dir (cwd-relative first, then gPathToResources).
	std::string basePath = RES_ResolveResourceDir("assets/locale/", "en.json");
	mgr->LoadStrings("en", basePath + "en.json");
	mgr->LoadStrings("pl", basePath + "pl.json");
	mgr->LoadStrings("it", basePath + "it.json");

	// --- Set active locale from saved preference, or default to English ---
	const char *savedLocale = "en";
	gApplicationDefaultConfig->GetString("locale", &savedLocale, "en");
	mgr->SetActiveLocale(savedLocale);

	LOGM("CDummyAppI18n::Init: i18n initialized with %d locales",
	     (int)mgr->GetRegisteredLocales().size());
}
