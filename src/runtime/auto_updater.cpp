#include "runtime/auto_updater.h"

#include "common/utils.h"
#include "version.h"

#include <Psapi.h>
#include <TlHelp32.h>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "Psapi.lib")

namespace toolscreen::auto_updater {

namespace {

constexpr const wchar_t* kReleasesApiUrl =
    L"https://api.github.com/repos/fe-art/Toolscreen/releases/latest";
constexpr const char* kDllName = "Toolscreen.dll";

std::atomic<bool> g_started{false};
std::atomic<bool> g_ready{false};
std::atomic<bool> g_applying{false};

std::mutex g_stateMutex;
std::string g_pendingVersion;
std::wstring g_pendingDllPath;
std::wstring g_liveDllPath;
std::wstring g_launcherExePath;
std::string g_instId;

bool HttpGet(const std::wstring& url, const std::wstring& userAgent, std::string& outBody, std::string& outError) {
    URL_COMPONENTS u{};
    u.dwStructSize = sizeof(u);
    u.dwSchemeLength = u.dwHostNameLength = u.dwUrlPathLength = u.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &u)) { outError = "WinHttpCrackUrl"; return false; }

    std::wstring host(u.lpszHostName, u.dwHostNameLength);
    std::wstring path(u.lpszUrlPath ? std::wstring(u.lpszUrlPath, u.dwUrlPathLength) : L"/");
    if (u.lpszExtraInfo && u.dwExtraInfoLength > 0) path.append(u.lpszExtraInfo, u.dwExtraInfoLength);

    HINTERNET hSession = WinHttpOpen(userAgent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) { outError = "WinHttpOpen"; return false; }
    WinHttpSetTimeouts(hSession, 5000, 5000, 15000, 30000);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), u.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); outError = "WinHttpConnect"; return false; }

    DWORD flags = (u.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); outError = "WinHttpOpenRequest"; return false; }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hReq, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

    bool ok = false;
    do {
        if (!WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) { outError = "WinHttpSendRequest"; break; }
        if (!WinHttpReceiveResponse(hReq, nullptr)) { outError = "WinHttpReceiveResponse"; break; }

        DWORD status = 0, statusSize = sizeof(status);
        if (!WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
            outError = "WinHttpQueryHeaders"; break;
        }
        if (status != 200) { outError = "HTTP " + std::to_string(status); break; }

        outBody.clear();
        DWORD avail = 0;
        do {
            if (!WinHttpQueryDataAvailable(hReq, &avail)) { outError = "WinHttpQueryDataAvailable"; break; }
            if (avail == 0) break;
            std::vector<char> buf(avail);
            DWORD read = 0;
            if (!WinHttpReadData(hReq, buf.data(), avail, &read)) { outError = "WinHttpReadData"; break; }
            outBody.append(buf.data(), read);
        } while (avail > 0);

        ok = outError.empty();
    } while (false);

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

bool ParseRelease(const std::string& body, std::string& outTag, std::string& outDllUrl) {
    try {
        auto j = nlohmann::json::parse(body, nullptr, /*allow_exceptions=*/false);
        if (j.is_discarded() || !j.is_object()) return false;
        if (!j.contains("tag_name") || !j["tag_name"].is_string()) return false;
        outTag = j["tag_name"].get<std::string>();

        if (!j.contains("assets") || !j["assets"].is_array()) return false;
        for (const auto& a : j["assets"]) {
            if (!a.is_object() || !a.contains("name") || !a["name"].is_string()) continue;
            std::string name = a["name"].get<std::string>();
            if (_stricmp(name.c_str(), kDllName) != 0) continue;
            if (!a.contains("browser_download_url") || !a["browser_download_url"].is_string()) continue;
            outDllUrl = a["browser_download_url"].get<std::string>();
            return !outDllUrl.empty();
        }
    } catch (...) {
        return false;
    }
    return false;
}

bool IsTrustedDownloadUrl(const std::string& url) {
    static const std::vector<std::string> trustedHosts = {
        "https://github.com/",
        "https://objects.githubusercontent.com/",
        "https://release-assets.githubusercontent.com/",
    };
    for (const auto& prefix : trustedHosts) {
        if (url.size() >= prefix.size() &&
            _strnicmp(url.c_str(), prefix.c_str(), prefix.size()) == 0) {
            return true;
        }
    }
    return false;
}

bool IsNewerVersion(const std::string& tag, const std::string& current) {
    std::regex re(R"(\D*(\d+)\.(\d+)\.(\d+))");
    std::smatch a, b;
    if (!std::regex_search(tag, a, re) || !std::regex_search(current, b, re)) return false;
    int ta = std::stoi(a[1]), tb = std::stoi(a[2]), tc = std::stoi(a[3]);
    int ca = std::stoi(b[1]), cb = std::stoi(b[2]), cc = std::stoi(b[3]);
    if (ta != ca) return ta > ca;
    if (tb != cb) return tb > cb;
    return tc > cc;
}

std::wstring GetThisDllPath() {
    HMODULE hMod = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)&GetThisDllPath, &hMod) ||
        !hMod) {
        return L"";
    }
    wchar_t buf[MAX_PATH * 2] = {0};
    DWORD len = GetModuleFileNameW(hMod, buf, sizeof(buf) / sizeof(wchar_t));
    if (!len) return L"";
    return std::wstring(buf, len);
}

std::wstring FindLauncherAncestor() {
    DWORD pid = GetCurrentProcessId();
    for (int hop = 0; hop < 8; ++hop) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) break;

        DWORD parent = 0;
        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (pe.th32ProcessID == pid) { parent = pe.th32ParentProcessID; break; }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        if (parent == 0) break;

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, parent);
        if (!hProc) { pid = parent; continue; }

        wchar_t path[MAX_PATH * 2] = {0};
        DWORD pathLen = sizeof(path) / sizeof(wchar_t);
        bool gotPath = QueryFullProcessImageNameW(hProc, 0, path, &pathLen) != 0;
        CloseHandle(hProc);

        if (gotPath) {
            std::wstring lower(path, pathLen);
            for (auto& c : lower) c = (wchar_t)towlower(c);
            if (lower.find(L"prismlauncher.exe") != std::wstring::npos ||
                lower.find(L"multimc.exe") != std::wstring::npos) {
                return std::wstring(path, pathLen);
            }
        }
        pid = parent;
    }
    return L"";
}

std::string ReadEnvVar(const char* name) {
    char buf[4096];
    DWORD len = GetEnvironmentVariableA(name, buf, sizeof(buf));
    if (len == 0 || len >= sizeof(buf)) return "";
    return std::string(buf, len);
}

void CheckAndDownload() {
    std::wstring liveDll = GetThisDllPath();
    if (liveDll.empty()) { Log("[Updater] cannot resolve own DLL path"); return; }

    std::wstring launcher = FindLauncherAncestor();
    std::string instId = ReadEnvVar("INST_ID");

    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_liveDllPath = liveDll;
        g_launcherExePath = launcher;
        g_instId = instId;
    }

    std::string body, err;
    if (!HttpGet(kReleasesApiUrl, L"Toolscreen-AutoUpdater/" TOOLSCREEN_VERSION_STRING, body, err)) {
        Log(std::string("[Updater] release fetch failed: ") + err);
        return;
    }

    std::string tag, dllUrl;
    if (!ParseRelease(body, tag, dllUrl)) {
        Log("[Updater] could not parse release JSON or no .dll asset");
        return;
    }
    if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) tag = tag.substr(1);

    std::string current = TOOLSCREEN_VERSION_STRING;
    if (!IsNewerVersion(tag, current)) {
        Log(std::string("[Updater] current=") + current + " latest=" + tag + " (no update)");
        return;
    }

    Log(std::string("[Updater] update available: ") + tag);

    if (!IsTrustedDownloadUrl(dllUrl)) {
        Log(std::string("[Updater] refusing untrusted download URL: ") + dllUrl);
        return;
    }

    std::string dllBody;
    if (!HttpGet(Utf8ToWide(dllUrl), L"Toolscreen-AutoUpdater/" TOOLSCREEN_VERSION_STRING, dllBody, err)) {
        Log(std::string("[Updater] DLL download failed: ") + err);
        return;
    }

    std::wstring pending = liveDll + L".pending";
    {
        std::ofstream out(std::filesystem::path(pending), std::ios::binary | std::ios::trunc);
        if (!out) { Log("[Updater] cannot open .pending for write"); return; }
        out.write(dllBody.data(), (std::streamsize)dllBody.size());
        if (!out) { Log("[Updater] write to .pending failed"); return; }
    }

    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_pendingVersion = tag;
        g_pendingDllPath = pending;
    }
    g_ready.store(true, std::memory_order_release);
    Log(std::string("[Updater] downloaded ") + tag + " to .pending; awaiting user apply");
}

} // namespace

void Start() {
    bool expected = false;
    if (!g_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
    Log("[Updater] starting check thread");
    std::thread([]() {
        try { CheckAndDownload(); }
        catch (const std::exception& e) { Log(std::string("[Updater] exception: ") + e.what()); }
        catch (...) { Log("[Updater] unknown exception"); }
    }).detach();
}

bool IsUpdateReady(std::string& outVersion) {
    if (!g_ready.load(std::memory_order_acquire)) return false;
    std::lock_guard<std::mutex> lock(g_stateMutex);
    outVersion = g_pendingVersion;
    return true;
}

bool ApplyAndRelaunch() {
    if (!g_ready.load(std::memory_order_acquire)) return false;
    bool expected = false;
    if (!g_applying.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        Log("[Updater] apply already in progress; ignoring duplicate click");
        return false;
    }

    std::wstring pending, live, launcher;
    std::string instId;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        pending = g_pendingDllPath;
        live = g_liveDllPath;
        launcher = g_launcherExePath;
        instId = g_instId;
    }
    if (pending.empty() || live.empty() || launcher.empty() || instId.empty()) {
        Log("[Updater] cannot apply: missing pending/live/launcher/instId");
        g_applying.store(false, std::memory_order_release);
        return false;
    }

    if (!std::regex_match(instId, std::regex(R"([A-Za-z0-9._ -]+)"))) {
        Log(std::string("[Updater] refusing unsafe INST_ID: ") + instId);
        g_applying.store(false, std::memory_order_release);
        return false;
    }

    DWORD selfPid = GetCurrentProcessId();
    std::wstring instIdW = Utf8ToWide(instId);
    std::wstring failFlag = live + L".update_failed.flag";

    std::wstring cmdLine =
        L"cmd.exe /c \"taskkill /F /PID " + std::to_wstring(selfPid) + L" >nul 2>&1"
        L" & timeout /T 2 /NOBREAK >nul"
        L" & ( move /Y \"" + pending + L"\" \"" + live + L"\""
        L"     && start \"\" \"" + launcher + L"\" --launch \"" + instIdW + L"\""
        L"   ) || ( echo failed > \"" + failFlag + L"\""
        L"          & start \"\" \"" + launcher + L"\" --launch \"" + instIdW + L"\""
        L"        )\"";

    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    // Hide banner before spawning helper; survives helper failure.
    g_ready.store(false, std::memory_order_release);

    BOOL ok = CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE,
                             CREATE_NO_WINDOW | DETACHED_PROCESS,
                             nullptr, nullptr, &si, &pi);
    if (!ok) {
        Log("[Updater] CreateProcess failed: " + std::to_string(GetLastError()));
        g_ready.store(true, std::memory_order_release);
        g_applying.store(false, std::memory_order_release);
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    Log("[Updater] helper spawned; this process will be killed shortly");
    return true;
}

} // namespace toolscreen::auto_updater
