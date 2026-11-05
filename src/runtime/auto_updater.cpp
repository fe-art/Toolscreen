#include "runtime/auto_updater.h"

#include "common/utils.h"
#include "runtime/auto_updater_script.h"
#include "runtime/jar_extract.h"
#include "runtime/pe_exports.h"
#include "runtime/release_select.h"
#include "runtime/signature_check.h"
#include "version.h"

#include <Psapi.h>
#include <TlHelp32.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "Psapi.lib")

namespace toolscreen::auto_updater {

namespace {

constexpr const wchar_t* kReleasesApiUrl =
    L"https://api.github.com/repos/jojoe77777/Toolscreen/releases?per_page=20";
constexpr const char* kTrustedDownloadPrefix = "https://github.com/jojoe77777/Toolscreen/releases/download/";
constexpr const wchar_t* kExpectedProductName = L"Toolscreen";
constexpr const char* kAgentEntryPoint = "Agent_OnLoad";

#ifdef TOOLSCREEN_UPDATER_TEST
constexpr const char* kCurrentVersion = "1.0.0";
constexpr const wchar_t* kTestAssumedSigner = L"C=US, S=Delaware, L=Lewes, O=SignPath Foundation, CN=SignPath Foundation";
#else
constexpr const char* kCurrentVersion = TOOLSCREEN_VERSION_STRING;
#endif

#if defined(_M_ARM64)
constexpr const char* kJarEntryName = "dlls/Toolscreen_arm64.dll";
constexpr WORD kExpectedMachine = IMAGE_FILE_MACHINE_ARM64;
#else
constexpr const char* kJarEntryName = "dlls/Toolscreen_x64.dll";
constexpr WORD kExpectedMachine = IMAGE_FILE_MACHINE_AMD64;
#endif

constexpr int kResolveTimeoutMs = 5000;
constexpr int kConnectTimeoutMs = 5000;
constexpr int kSendTimeoutMs = 15000;
constexpr int kReceiveTimeoutMs = 30000;
constexpr int kMaxLauncherHops = 8;
constexpr DWORD kEnvBufferSize = 4096;
constexpr int kCloseWaitPolls = 20;
constexpr int kMaxApplyLogLines = 40;
constexpr size_t kMaxApiBytes = 4u * 1024u * 1024u;
constexpr size_t kMaxDownloadBytes = 96u * 1024u * 1024u;
constexpr size_t kMaxDllBytes = 64u * 1024u * 1024u;

std::atomic<bool> g_started{false};
std::atomic<bool> g_ready{false};
std::atomic<bool> g_applying{false};
std::atomic<bool> g_active{false};

std::mutex g_stateMutex;
std::string g_pendingVersion;
bool g_pendingIsBeta = false;
std::wstring g_pendingDllPath;
std::wstring g_liveDllPath;
std::wstring g_launcherExePath;
std::string g_instId;

bool HttpGet(const std::wstring& url, const std::wstring& userAgent, size_t maxBytes, std::string& outBody, std::string& outError) {
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
    WinHttpSetTimeouts(hSession, kResolveTimeoutMs, kConnectTimeoutMs, kSendTimeoutMs, kReceiveTimeoutMs);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), u.nPort, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); outError = "WinHttpConnect"; return false; }

    DWORD flags = (u.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), nullptr,
                                        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); outError = "WinHttpOpenRequest"; return false; }

    DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
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
            if (outBody.size() + read > maxBytes) { outError = "response too large"; break; }
            outBody.append(buf.data(), read);
        } while (avail > 0);

        ok = outError.empty();
    } while (false);

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

bool IsTrustedDownloadUrl(const std::string& url) {
    const std::string prefix = kTrustedDownloadPrefix;
    return url.size() > prefix.size() && _strnicmp(url.c_str(), prefix.c_str(), prefix.size()) == 0;
}

bool PeMachineMatches(const std::wstring& filePath) {
    std::ifstream in(std::filesystem::path(filePath), std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    if (!in.read(reinterpret_cast<char*>(&dos), sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE) return false;
    in.seekg(dos.e_lfanew, std::ios::beg);
    DWORD signature = 0;
    IMAGE_FILE_HEADER header{};
    if (!in.read(reinterpret_cast<char*>(&signature), sizeof(signature)) || signature != IMAGE_NT_SIGNATURE) return false;
    if (!in.read(reinterpret_cast<char*>(&header), sizeof(header))) return false;
    return header.Machine == kExpectedMachine;
}

bool IsCmdSafePath(const std::wstring& p) {
    return p.find(L'"') == std::wstring::npos && p.find(L'%') == std::wstring::npos;
}

bool IsSafeInstanceId(const std::string& instId) {
    static const std::regex pattern(R"([A-Za-z0-9._ -]+)");
    return std::regex_match(instId, pattern);
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
    if (!len || len >= sizeof(buf) / sizeof(wchar_t)) return L"";
    return std::wstring(buf, len);
}

std::wstring FindLauncherAncestor() {
    DWORD pid = GetCurrentProcessId();
    for (int hop = 0; hop < kMaxLauncherHops; ++hop) {
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
    char buf[kEnvBufferSize];
    DWORD len = GetEnvironmentVariableA(name, buf, sizeof(buf));
    if (len == 0 || len >= sizeof(buf)) return "";
    return std::string(buf, len);
}

bool CanApplyHere(const std::wstring& live, const std::wstring& launcher, const std::string& instId) {
    if (live.empty() || launcher.empty() || instId.empty()) {
        Log("[Updater] cannot apply here: no PrismLauncher/MultiMC ancestor or no INST_ID");
        return false;
    }
    if (!IsSafeInstanceId(instId)) {
        Log(std::string("[Updater] refusing unsafe INST_ID: ") + instId);
        return false;
    }
    if (!IsCmdSafePath(live) || !IsCmdSafePath(launcher)) {
        Log("[Updater] cannot apply here: a path contains characters unsafe for the apply script");
        return false;
    }
    return true;
}

std::wstring FailureFlagPath(const std::wstring& liveDll) {
    return liveDll + L".update_failed.flag";
}

std::wstring ApplyLogPath(const std::wstring& liveDll) {
    return liveDll + L".update.log";
}

void ReportAndClearPreviousApply(const std::wstring& liveDll) {
    std::error_code ec;
    std::filesystem::remove(std::filesystem::path(liveDll + L".pending"), ec);

    const std::filesystem::path logFile(ApplyLogPath(liveDll));
    if (std::filesystem::exists(logFile, ec)) {
        std::ifstream in(logFile);
        std::string line;
        for (int shown = 0; shown < kMaxApplyLogLines && std::getline(in, line); ++shown) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            Log("[Updater apply] " + line);
        }
        in.close();
        std::filesystem::remove(logFile, ec);
    }

    const std::filesystem::path flag(FailureFlagPath(liveDll));
    if (!std::filesystem::exists(flag, ec)) return;
    std::string reason;
    {
        std::ifstream in(flag);
        std::getline(in, reason);
    }
    Log("[Updater] previous update did not apply: " + reason);
    std::filesystem::remove(flag, ec);
}

void CheckAndDownload(bool includeBetas) {
    std::wstring liveDll = GetThisDllPath();
    if (liveDll.empty()) { Log("[Updater] cannot resolve own DLL path"); return; }

    ReportAndClearPreviousApply(liveDll);

    std::wstring ownSigner;
    if (!VerifySignedAndGetSigner(liveDll, false, ownSigner)) {
#ifdef TOOLSCREEN_UPDATER_TEST
        ownSigner = kTestAssumedSigner;
        Log("[Updater] TEST BUILD: running DLL is unsigned, trusting the release publisher and pretending to be version 1.0.0");
#else
        Log("[Updater] running DLL is not validly signed (development build); updater disabled");
        return;
#endif
    }

    std::wstring launcher = FindLauncherAncestor();
    std::string instId = ReadEnvVar("INST_ID");
    if (!CanApplyHere(liveDll, launcher, instId)) return;
    Log(std::string("[Updater] can apply here: launcher found, instance ") + instId);
    g_active.store(true, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_liveDllPath = liveDll;
        g_launcherExePath = launcher;
        g_instId = instId;
    }

    const std::wstring userAgent = L"Toolscreen-AutoUpdater/" TOOLSCREEN_VERSION_STRING;

    std::string body, err;
    if (!HttpGet(kReleasesApiUrl, userAgent, kMaxApiBytes, body, err)) {
        Log(std::string("[Updater] release fetch failed: ") + err);
        return;
    }

    Log("[Updater] release list fetched, " + std::to_string(body.size()) + " bytes");

    ReleaseChoice release;
    if (!SelectRelease(body, includeBetas, release)) {
        Log("[Updater] could not parse the release list or find a release JAR asset");
        return;
    }
    std::string tag = release.tag;
    const std::string jarUrl = release.jarUrl;
    Log("[Updater] newest eligible release: " + release.tag + (release.isBeta ? " (beta)" : ""));
    if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V')) tag = tag.substr(1);

    const std::string current = kCurrentVersion;
    if (!IsNewerVersion(tag, current)) {
        Log(std::string("[Updater] current=") + current + " latest=" + tag + " (no update)");
        return;
    }

    Log(std::string("[Updater] update available: ") + tag);

    if (!IsTrustedDownloadUrl(jarUrl)) {
        Log(std::string("[Updater] refusing untrusted download URL: ") + jarUrl);
        return;
    }

    std::string dllBytes;
    {
        std::string jarBody;
        if (!HttpGet(Utf8ToWide(jarUrl), userAgent, kMaxDownloadBytes, jarBody, err)) {
            Log(std::string("[Updater] JAR download failed: ") + err);
            return;
        }
        Log("[Updater] release JAR downloaded, " + std::to_string(jarBody.size()) + " bytes");
        if (!ExtractZipEntry(jarBody, kJarEntryName, kMaxDllBytes, dllBytes)) {
            Log(std::string("[Updater] release JAR has no usable ") + kJarEntryName);
            return;
        }
        Log(std::string("[Updater] extracted ") + kJarEntryName + ", " + std::to_string(dllBytes.size()) + " bytes");
    }

    std::wstring pending = liveDll + L".pending";
    {
        std::ofstream out(std::filesystem::path(pending), std::ios::binary | std::ios::trunc);
        if (!out) { Log("[Updater] cannot open .pending for write"); return; }
        out.write(dllBytes.data(), (std::streamsize)dllBytes.size());
        if (!out) { Log("[Updater] write to .pending failed"); return; }
    }

    const auto discardPending = [&](const char* reason) {
        Log(std::string("[Updater] ") + reason + "; discarding");
        std::error_code ec;
        std::filesystem::remove(std::filesystem::path(pending), ec);
    };

    if (!PeMachineMatches(pending)) { discardPending("downloaded DLL is for a different CPU architecture"); return; }
    if (!pe::ExportsSymbol(pending, kAgentEntryPoint)) { discardPending("downloaded DLL does not export Agent_OnLoad and could not be loaded as an agent"); return; }

    std::wstring newSigner;
    if (!VerifySignedAndGetSigner(pending, true, newSigner)) {
        discardPending("downloaded DLL failed Authenticode verification");
        return;
    }
    if (newSigner != ownSigner) { discardPending("downloaded DLL is signed by a different publisher"); return; }
    if (ReadProductName(pending) != kExpectedProductName) { discardPending("downloaded DLL is not a Toolscreen build"); return; }
    Log("[Updater] downloaded DLL passed the architecture, agent export, signature, publisher and product checks");

    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_pendingVersion = tag;
        g_pendingIsBeta = release.isBeta;
        g_pendingDllPath = pending;
    }
    g_ready.store(true, std::memory_order_release);
    Log(std::string("[Updater] downloaded ") + tag + (release.isBeta ? " (beta)" : "") + " to .pending; awaiting user apply");
}

bool WriteApplyScript(const std::wstring& scriptPath, const std::wstring& script) {
    std::ofstream out(std::filesystem::path(scriptPath), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    const std::string utf8 = WideToUtf8(script);
    out.write(utf8.data(), (std::streamsize)utf8.size());
    return static_cast<bool>(out);
}

std::wstring SystemDirectory() {
    wchar_t buf[MAX_PATH] = {0};
    UINT len = GetSystemDirectoryW(buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return L"";
    return std::wstring(buf, len);
}

std::wstring ApplyScriptPath(DWORD pid) {
    wchar_t buf[MAX_PATH] = {0};
    DWORD len = GetTempPathW(MAX_PATH, buf);
    if (len == 0 || len >= MAX_PATH) return L"";
    return std::wstring(buf, len) + L"toolscreen_update_" + std::to_wstring(pid) + L".cmd";
}

} // namespace

void Start(bool includeBetas) {
    bool expected = false;
    if (!g_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;
    Log(std::string("[Updater] starting check thread, betas ") + (includeBetas ? "on" : "off"));
    try {
        std::thread([includeBetas]() {
            try { CheckAndDownload(includeBetas); }
            catch (const std::exception& e) { Log(std::string("[Updater] exception: ") + e.what()); }
            catch (...) { Log("[Updater] unknown exception"); }
        }).detach();
    } catch (...) {
        Log("[Updater] could not start check thread");
    }
}

bool IsActive() {
    return g_active.load(std::memory_order_acquire);
}

bool IsUpdateReady(std::string& outVersion, bool& outIsBeta) {
    if (!g_ready.load(std::memory_order_acquire)) return false;
    std::lock_guard<std::mutex> lock(g_stateMutex);
    outVersion = g_pendingVersion;
    outIsBeta = g_pendingIsBeta;
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

    const DWORD selfPid = GetCurrentProcessId();
    const std::wstring scriptPath = ApplyScriptPath(selfPid);
    const std::wstring systemDir = SystemDirectory();

    if (pending.empty() || scriptPath.empty() || systemDir.empty() || !IsCmdSafePath(scriptPath) ||
        !CanApplyHere(live, launcher, instId)) {
        Log("[Updater] cannot apply: missing or unsafe paths");
        g_applying.store(false, std::memory_order_release);
        return false;
    }

    ApplyScriptParams params;
    params.gamePid = selfPid;
    params.closeWaitPolls = kCloseWaitPolls;
    params.pendingDll = pending;
    params.liveDll = live;
    params.launcherExe = launcher;
    params.instanceId = Utf8ToWide(instId);
    params.failureFlag = FailureFlagPath(live);
    params.logFile = ApplyLogPath(live);

    if (!WriteApplyScript(scriptPath, BuildApplyScript(params))) {
        Log("[Updater] cannot write the apply script");
        g_applying.store(false, std::memory_order_release);
        return false;
    }

    std::wstring cmdLine = L"cmd.exe /c \"\"" + scriptPath + L"\"\"";
    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(L'\0');
    const std::wstring cmdExe = systemDir + L"\\cmd.exe";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};

    g_ready.store(false, std::memory_order_release);

    BOOL ok = CreateProcessW(cmdExe.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE,
                             CREATE_NO_WINDOW,
                             nullptr, systemDir.c_str(), &si, &pi);
    if (!ok) {
        Log("[Updater] CreateProcess failed: " + std::to_string(GetLastError()));
        std::error_code ec;
        std::filesystem::remove(std::filesystem::path(scriptPath), ec);
        g_ready.store(true, std::memory_order_release);
        g_applying.store(false, std::memory_order_release);
        return false;
    }
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    Log("[Updater] apply script started (pid " + std::to_string(selfPid) + "); it will close the game, swap the DLL and relaunch");
    return true;
}

} // namespace toolscreen::auto_updater
