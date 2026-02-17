#include "i18n.h"
#include "gui.h"
#include "toml.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <Windows.h>

// Translation table: English string -> translated string.
// Written only from the GUI thread (language switch), read only from the GUI thread (rendering).
// No mutex needed.
static std::unordered_map<std::string, std::string> g_translations;

const char* TR(const char* english) {
    if (!english) { return english; }
    auto it = g_translations.find(english);
    if (it != g_translations.end()) { return it->second.c_str(); }
    return english;
}

bool LoadLanguage(const std::string& lang) {
    g_translations.clear();

    // English is the built-in fallback — no resource needed.
    if (lang.empty() || lang == "en") {
        Log("i18n: English fallback active");
        return false;
    }

    // Find the LANGFILE resource whose name matches the language code (uppercased).
    HMODULE hModule = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&LoadLanguage), &hModule);
    if (!hModule) {
        Log("i18n: Failed to get module handle, falling back to English.");
        return false;
    }

    std::wstring resName(lang.begin(), lang.end());
    for (auto& c : resName) c = towupper(c);

    HRSRC hRes = FindResourceW(hModule, resName.c_str(), L"LANGFILE");
    if (!hRes) {
        Log("i18n: No built-in translation for language: " + lang);
        return false;
    }

    HGLOBAL hData = LoadResource(hModule, hRes);
    DWORD size = SizeofResource(hModule, hRes);
    const char* data = static_cast<const char*>(LockResource(hData));
    if (!data || size == 0) {
        Log("i18n: Failed to read LANGFILE resource for: " + lang);
        return false;
    }

    try {
        toml::table result = toml::parse(std::string_view{ data, size });
        int count = 0;
        for (auto& [key, value] : result) {
            if (auto str = value.value<std::string>()) {
                g_translations[std::string(key.str())] = *str;
                ++count;
            }
        }
        Log("i18n: Loaded " + std::to_string(count) + " translations for language: " + lang);
        return true;
    } catch (const std::exception& e) {
        Log("i18n: Failed to parse translations for " + lang + ": " + std::string(e.what()));
        g_translations.clear();
        return false;
    }
}
