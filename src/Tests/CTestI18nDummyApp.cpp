#include "CTestI18nDummyApp.h"
#include "CI18nManager.h"
#include "DBG_Log.h"
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// Assertion helpers (local to this TU)
// ---------------------------------------------------------------------------

#define I18N_ASSERT_EQ(actual, expected, msg) \
	do { \
		if ((actual) != (expected)) { \
			char _buf[512]; \
			snprintf(_buf, sizeof(_buf), "FAIL: %s — expected '%s', got '%s'", \
				msg, std::string(expected).c_str(), std::string(actual).c_str()); \
			LOGD("CTestI18nDummyApp: %s", _buf); \
			TestCompleted(false, _buf); \
			return; \
		} \
		StepCompleted(stepNum++, true, msg); \
	} while(0)

#define I18N_ASSERT_TRUE(cond, msg) \
	do { \
		if (!(cond)) { \
			char _buf[256]; \
			snprintf(_buf, sizeof(_buf), "FAIL: %s", msg); \
			LOGD("CTestI18nDummyApp: %s", _buf); \
			TestCompleted(false, _buf); \
			return; \
		} \
		StepCompleted(stepNum++, true, msg); \
	} while(0)

// ---------------------------------------------------------------------------

CTestI18nDummyApp::CTestI18nDummyApp()  {}
CTestI18nDummyApp::~CTestI18nDummyApp() {}

void CTestI18nDummyApp::Run(ITestCallback *callback)
{
	this->callback = callback;
	isRunning = true;
	int stepNum = 1;

	LOGD("CTestI18nDummyApp: Starting DummyApp i18n tests");

	CI18nManager *mgr = CI18nManager::Instance();

	// =========================================================================
	// Test 1: Three locales registered
	// =========================================================================
	{
		const auto &locales = mgr->GetRegisteredLocales();
		bool hasEN = false, hasPL = false, hasIT = false;
		for (const auto &loc : locales)
		{
			if (loc.tag == "en") hasEN = true;
			if (loc.tag == "pl") hasPL = true;
			if (loc.tag == "it") hasIT = true;
		}
		I18N_ASSERT_TRUE(hasEN, "English locale registered");
		I18N_ASSERT_TRUE(hasPL, "Polish locale registered");
		I18N_ASSERT_TRUE(hasIT, "Italian locale registered");
	}

	// =========================================================================
	// Test 2: Default locale is set (must be one of the registered ones)
	// =========================================================================
	{
		const std::string &active = mgr->GetActiveLocale();
		bool valid = (active == "en" || active == "pl" || active == "it");
		I18N_ASSERT_TRUE(valid, "Active locale is a registered locale");
	}

	// =========================================================================
	// Test 3: English string table loaded — key resolves to translated value
	// =========================================================================
	{
		mgr->SetActiveLocale("en");
		const char *val = mgr->Get("menu.language");
		I18N_ASSERT_TRUE(val != nullptr && strcmp(val, "menu.language") != 0,
		                 "EN: menu.language resolved");
		I18N_ASSERT_EQ(std::string(val), std::string("Language"),
		               "EN: menu.language == 'Language'");
	}

	// =========================================================================
	// Test 4: Polish string table loaded
	// =========================================================================
	{
		mgr->SetActiveLocale("pl");
		const char *val = mgr->Get("menu.language");
		I18N_ASSERT_TRUE(val != nullptr && strcmp(val, "menu.language") != 0,
		                 "PL: menu.language resolved");
		I18N_ASSERT_EQ(std::string(val), std::string("Język"),
		               "PL: menu.language == 'Język'");
	}

	// =========================================================================
	// Test 5: Italian string table loaded
	// =========================================================================
	{
		mgr->SetActiveLocale("it");
		const char *val = mgr->Get("menu.language");
		I18N_ASSERT_TRUE(val != nullptr && strcmp(val, "menu.language") != 0,
		                 "IT: menu.language resolved");
		I18N_ASSERT_EQ(std::string(val), std::string("Lingua"),
		               "IT: menu.language == 'Lingua'");
	}

	// =========================================================================
	// Test 6: Fallback — missing key in PL falls back to EN
	// =========================================================================
	{
		// Inject a key that only exists in EN
		mgr->SetString("en", "test.fallback_only_en", "FallbackValue");

		mgr->SetActiveLocale("pl");
		const char *val = mgr->Get("test.fallback_only_en");
		I18N_ASSERT_TRUE(val != nullptr && strcmp(val, "test.fallback_only_en") != 0,
		                 "PL falls back to EN for missing key");
		I18N_ASSERT_EQ(std::string(val), std::string("FallbackValue"),
		               "PL fallback value matches EN value");
	}

	// =========================================================================
	// Test 7: Plural rules — English cardinal
	// =========================================================================
	{
		mgr->SetActiveLocale("en");
		I18N_ASSERT_TRUE(mgr->GetPluralCategory(1.0) == I18N_PLURAL_ONE,
		                 "EN plural: 1 -> one");
		I18N_ASSERT_TRUE(mgr->GetPluralCategory(2.0) == I18N_PLURAL_OTHER,
		                 "EN plural: 2 -> other");
	}

	// =========================================================================
	// Test 8: Plural rules — Polish cardinal
	// =========================================================================
	{
		mgr->SetActiveLocale("pl");
		I18N_ASSERT_TRUE(mgr->GetPluralCategory(1.0) == I18N_PLURAL_ONE,
		                 "PL plural: 1 -> one");
		I18N_ASSERT_TRUE(mgr->GetPluralCategory(2.0) == I18N_PLURAL_FEW,
		                 "PL plural: 2 -> few");
		I18N_ASSERT_TRUE(mgr->GetPluralCategory(5.0) == I18N_PLURAL_MANY,
		                 "PL plural: 5 -> many");
	}

	// =========================================================================
	// Test 8b: Built-in CLDR rule getters (engine API) — incl. German
	// =========================================================================
	{
		I18N_ASSERT_TRUE(CI18nManager::GetBuiltinCardinalRule("en") != nullptr,
		                 "Builtin cardinal rule exists for en");
		I18N_ASSERT_TRUE(CI18nManager::GetBuiltinCardinalRule("pl") != nullptr,
		                 "Builtin cardinal rule exists for pl");
		I18N_ASSERT_TRUE(CI18nManager::GetBuiltinCardinalRule("it") != nullptr,
		                 "Builtin cardinal rule exists for it");
		I18N_ASSERT_TRUE(CI18nManager::GetBuiltinCardinalRule("de") != nullptr,
		                 "Builtin cardinal rule exists for de");
		I18N_ASSERT_TRUE(CI18nManager::GetBuiltinCardinalRule("xx") == nullptr,
		                 "Builtin cardinal rule absent for unknown language");

		// German cardinal: i=1 & v=0 -> one, else other. Invoke the rule directly
		// (German is not a registered app locale).
		EI18nPluralRuleFn deCard = CI18nManager::GetBuiltinCardinalRule("de");
		I18N_ASSERT_TRUE(deCard(1, 1, 0, 0, 0, 0) == I18N_PLURAL_ONE,
		                 "DE cardinal: 1 -> one");
		I18N_ASSERT_TRUE(deCard(2, 2, 0, 0, 0, 0) == I18N_PLURAL_OTHER,
		                 "DE cardinal: 2 -> other");

		// Primary-subtag extraction: "en-US" resolves to the English rule.
		I18N_ASSERT_TRUE(CI18nManager::GetBuiltinCardinalRule("en-US") != nullptr,
		                 "Builtin cardinal rule resolves for en-US");

		// German and Polish have no dedicated ordinal rule (all -> other).
		I18N_ASSERT_TRUE(CI18nManager::GetBuiltinOrdinalRule("de") == nullptr,
		                 "DE has no dedicated ordinal rule");
		I18N_ASSERT_TRUE(CI18nManager::GetBuiltinOrdinalRule("en") != nullptr,
		                 "EN has an ordinal rule");
	}

	// =========================================================================
	// Test 9: All required menu keys exist in every locale
	// =========================================================================
	{
		const char *locales[] = { "en", "pl", "it" };
		const char *keys[] = {
			"menu.file",
			"menu.file.quit",
			"menu.workspace",
			"menu.workspace.new",
			"menu.workspace.delete",
			"menu.settings",
			"menu.settings.theme",
			"menu.settings.renderer",
			"menu.settings.renderer.running",
			"menu.settings.renderer.restart",
			"menu.settings.renderer.only_one",
			"menu.settings.renderer.cli_override",
			"menu.settings.gui_scale",
			"menu.settings.renderer.restart_title",
			"menu.settings.renderer.restart_message",
			"menu.examples",
			"menu.examples.music_player",
			"menu.examples.ai",
			"menu.examples.image_loader",
			"menu.examples.camera",
			"menu.examples.hdr_test",
			"menu.examples.video_player",
			"menu.examples.video_player.unavailable",
			"menu.examples.i18n",
			"menu.help",
			"menu.language",
			// Keys the ENGINE hands back (CImageData::GetHeifAvailabilityI18nKey)
			// rather than ones this app invents. They are listed here for the
			// same reason as the rest: the engine returns a key and it is the
			// HOST's job to have a translation for it, so a missing one is this
			// app's bug and this is where that gets caught.
			"engine.heif.unavailable.system_codec_missing",
			"engine.heif.unavailable.not_built",
			"heif.install_link",
		};

		for (const char *locale : locales)
		{
			mgr->SetActiveLocale(locale);
			for (const char *key : keys)
			{
				const char *val = mgr->Get(key);
				char msg[256];
				snprintf(msg, sizeof(msg), "%s: key '%s' resolved", locale, key);
				I18N_ASSERT_TRUE(val != nullptr && strcmp(val, key) != 0, msg);
			}
		}
	}

	// Restore locale to English
	mgr->SetActiveLocale("en");

	LOGD("CTestI18nDummyApp: All %d steps passed", stepNum - 1);
	TestCompleted(true, "All DummyApp i18n tests passed");
}

void CTestI18nDummyApp::Cancel()
{
	isRunning = false;
}
