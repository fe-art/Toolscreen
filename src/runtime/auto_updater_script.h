#pragma once

#include <string>

namespace toolscreen::auto_updater {

struct ApplyScriptParams {
    unsigned long gamePid = 0;
    int closeWaitPolls = 0;
    std::wstring pendingDll;
    std::wstring liveDll;
    std::wstring launcherExe;
    std::wstring instanceId;
    std::wstring failureFlag;
    std::wstring logFile;
};

inline std::wstring BuildApplyScript(const ApplyScriptParams& p) {
    const std::wstring pid = std::to_wstring(p.gamePid);
    const std::wstring polls = std::to_wstring(p.closeWaitPolls);
    const std::wstring log = L">>\"" + p.logFile + L"\" echo %date% %time% ";
    std::wstring s;
    s += L"@echo off\r\n";
    s += L"chcp 65001 >nul\r\n";
    s += log + L"apply script started, waiting for pid " + pid + L" to close\r\n";
    s += L"taskkill /PID " + pid + L" >nul 2>&1\r\n";
    s += log + L"taskkill exit=%errorlevel%\r\n";
    s += L"set /a waited=0\r\n";
    s += L":wait\r\n";
    s += L"tasklist /FI \"PID eq " + pid + L"\" /NH 2>nul | find \" " + pid + L" \" >nul\r\n";
    s += L"if errorlevel 1 goto closed\r\n";
    s += L"set /a waited+=1\r\n";
    s += L"if %waited% GEQ " + polls + L" goto stuck\r\n";
    s += L"timeout /T 1 /NOBREAK >nul\r\n";
    s += L"goto wait\r\n";
    s += L":stuck\r\n";
    s += log + L"game still running after %waited% polls, giving up\r\n";
    s += L"echo game_did_not_close> \"" + p.failureFlag + L"\"\r\n";
    s += L"goto finish\r\n";
    s += L":closed\r\n";
    s += log + L"game closed after %waited% polls\r\n";
    s += L"move /Y \"" + p.pendingDll + L"\" \"" + p.liveDll + L"\" >nul 2>&1\r\n";
    s += L"if errorlevel 1 (echo move_failed> \"" + p.failureFlag + L"\" & " + log + L"move FAILED) else (" + log + L"move ok)\r\n";
    s += L"start \"\" \"" + p.launcherExe + L"\" --launch \"" + p.instanceId + L"\"\r\n";
    s += log + L"relaunch requested\r\n";
    s += L":finish\r\n";
    s += L"(goto) 2>nul & del \"%~f0\"\r\n";
    return s;
}

} // namespace toolscreen::auto_updater
