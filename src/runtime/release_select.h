#pragma once

#include <cstring>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>
#include <utility>

namespace toolscreen::auto_updater {

inline constexpr const char* kJarAssetPrefix = "Toolscreen-";
inline constexpr const char* kJarAssetSuffix = "-double-click-me.jar";

inline bool IsNewerVersion(const std::string& tag, const std::string& current) {
    static const std::regex re(R"(\D*(\d+)\.(\d+)\.(\d+))");
    std::smatch a, b;
    if (!std::regex_search(tag, a, re) || !std::regex_search(current, b, re)) return false;
    int ta = std::stoi(a[1]), tb = std::stoi(a[2]), tc = std::stoi(a[3]);
    int ca = std::stoi(b[1]), cb = std::stoi(b[2]), cc = std::stoi(b[3]);
    if (ta != ca) return ta > ca;
    if (tb != cb) return tb > cb;
    return tc > cc;
}

inline bool IsReleaseJarName(const std::string& name) {
    const std::string prefix = kJarAssetPrefix;
    const std::string suffix = kJarAssetSuffix;
    if (name.size() <= prefix.size() + suffix.size()) return false;
    if (_strnicmp(name.c_str(), prefix.c_str(), prefix.size()) != 0) return false;
    return _stricmp(name.c_str() + name.size() - suffix.size(), suffix.c_str()) == 0;
}

struct ReleaseChoice {
    std::string tag;
    std::string jarUrl;
    bool isBeta = false;
};

inline bool JsonFlag(const nlohmann::json& object, const char* key) {
    return object.contains(key) && object[key].is_boolean() && object[key].get<bool>();
}

inline bool FindJarAssetUrl(const nlohmann::json& release, std::string& outJarUrl) {
    if (!release.contains("assets") || !release["assets"].is_array()) return false;
    for (const auto& a : release["assets"]) {
        if (!a.is_object() || !a.contains("name") || !a["name"].is_string()) continue;
        if (!IsReleaseJarName(a["name"].get<std::string>())) continue;
        if (!a.contains("browser_download_url") || !a["browser_download_url"].is_string()) continue;
        outJarUrl = a["browser_download_url"].get<std::string>();
        if (!outJarUrl.empty()) return true;
    }
    return false;
}

inline bool SelectRelease(const std::string& body, bool includeBetas, ReleaseChoice& out) {
    try {
        auto releases = nlohmann::json::parse(body, nullptr, false);
        if (releases.is_discarded() || !releases.is_array()) return false;
        bool found = false;
        for (const auto& r : releases) {
            if (!r.is_object() || JsonFlag(r, "draft")) continue;
            const bool prerelease = JsonFlag(r, "prerelease");
            if (prerelease && !includeBetas) continue;
            if (!r.contains("tag_name") || !r["tag_name"].is_string()) continue;
            ReleaseChoice candidate;
            candidate.tag = r["tag_name"].get<std::string>();
            candidate.isBeta = prerelease;
            if (!FindJarAssetUrl(r, candidate.jarUrl)) continue;
            if (!found || IsNewerVersion(candidate.tag, out.tag)) {
                out = std::move(candidate);
                found = true;
            }
        }
        return found;
    } catch (...) {
        return false;
    }
}

} // namespace toolscreen::auto_updater
