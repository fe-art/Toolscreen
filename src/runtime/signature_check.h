#pragma once

#include <cstdio>
#include <string>
#include <vector>
#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>

#pragma comment(lib, "wintrust.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "Version.lib")

namespace toolscreen::auto_updater {

inline bool VerifySignedAndGetSigner(const std::wstring& filePath, bool checkRevocation, std::wstring& outSubject) {
    WINTRUST_FILE_INFO fileInfo{};
    fileInfo.cbStruct = sizeof(fileInfo);
    fileInfo.pcwszFilePath = filePath.c_str();

    GUID action = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA wd{};
    wd.cbStruct = sizeof(wd);
    wd.dwUIChoice = WTD_UI_NONE;
    wd.fdwRevocationChecks = checkRevocation ? WTD_REVOKE_WHOLECHAIN : WTD_REVOKE_NONE;
    wd.dwUnionChoice = WTD_CHOICE_FILE;
    wd.pFile = &fileInfo;
    wd.dwStateAction = WTD_STATEACTION_VERIFY;
    wd.dwProvFlags = WTD_SAFER_FLAG;

    const LONG status = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &wd);
    bool ok = false;
    if (status == ERROR_SUCCESS) {
        CRYPT_PROVIDER_DATA* provider = WTHelperProvDataFromStateData(wd.hWVTStateData);
        CRYPT_PROVIDER_SGNR* signer = provider ? WTHelperGetProvSignerFromChain(provider, 0, FALSE, 0) : nullptr;
        CRYPT_PROVIDER_CERT* cert = signer ? WTHelperGetProvCertFromChain(signer, 0) : nullptr;
        if (cert && cert->pCert) {
            DWORD nameType = CERT_X500_NAME_STR;
            const DWORD length = CertGetNameStringW(cert->pCert, CERT_NAME_RDN_TYPE, 0, &nameType, nullptr, 0);
            if (length > 1) {
                std::wstring name(length, L'\0');
                CertGetNameStringW(cert->pCert, CERT_NAME_RDN_TYPE, 0, &nameType, name.data(), length);
                name.resize(length - 1);
                outSubject = name;
                ok = true;
            }
        }
    }
    wd.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action, &wd);
    return ok;
}

inline std::wstring ReadProductName(const std::wstring& filePath) {
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(filePath.c_str(), &ignored);
    if (size == 0) return L"";
    std::vector<char> data(size);
    if (!GetFileVersionInfoW(filePath.c_str(), 0, size, data.data())) return L"";

    struct LangCodePage {
        WORD language;
        WORD codePage;
    };
    LangCodePage* translations = nullptr;
    UINT translationBytes = 0;
    if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<LPVOID*>(&translations),
                        &translationBytes) ||
        translationBytes < sizeof(LangCodePage)) {
        return L"";
    }

    wchar_t key[64];
    swprintf_s(key, L"\\StringFileInfo\\%04x%04x\\ProductName", translations[0].language, translations[0].codePage);
    wchar_t* value = nullptr;
    UINT valueLength = 0;
    if (!VerQueryValueW(data.data(), key, reinterpret_cast<LPVOID*>(&value), &valueLength) || !value) return L"";
    return std::wstring(value);
}

} // namespace toolscreen::auto_updater
