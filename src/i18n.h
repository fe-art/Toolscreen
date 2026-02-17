#pragma once

#include <string>

// Returns the translation of 'english' in the current language,
// or 'english' itself if no translation is found (English fallback).
// Only call from the GUI thread.
const char* TR(const char* english);

// Loads translations for the given language code from the embedded LANGFILE
// resource in the DLL. Pass "en" to clear and use English fallback.
// Returns true if translations were loaded, false if the language resource was
// not found (or lang is "en"). Only call from the GUI thread.
bool LoadLanguage(const std::string& lang);
