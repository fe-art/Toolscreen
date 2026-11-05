#include "runtime/pe_exports.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void Check(bool condition, const char* label) {
    if (!condition) {
        std::cerr << "  ASSERT FAILED: " << label << "\n";
        ++g_failures;
    }
}

std::wstring SystemDll() {
    wchar_t directory[MAX_PATH];
    const UINT length = GetSystemDirectoryW(directory, MAX_PATH);
    return std::wstring(directory, length) + L"\\kernel32.dll";
}

std::filesystem::path TempFile(const char* suffix) {
    return std::filesystem::temp_directory_path() / (std::string("pe_exports_test_") + std::to_string(GetCurrentProcessId()) + suffix);
}

void FindsExportedSymbol() {
    Check(toolscreen::pe::ExportsSymbol(SystemDll(), "GetLastError"), "kernel32 exports GetLastError");
    Check(toolscreen::pe::ExportsSymbol(SystemDll(), "CreateFileW"), "kernel32 exports CreateFileW");
}

void RejectsUnexportedSymbol() {
    Check(!toolscreen::pe::ExportsSymbol(SystemDll(), "Agent_OnLoad"), "kernel32 has no Agent_OnLoad");
}

void RequiresExactName() {
    Check(!toolscreen::pe::ExportsSymbol(SystemDll(), "GetLastErro"), "prefix of an export is not an export");
    Check(!toolscreen::pe::ExportsSymbol(SystemDll(), "GetLastErrorX"), "extension of an export is not an export");
    Check(!toolscreen::pe::ExportsSymbol(SystemDll(), "getlasterror"), "names are case sensitive");
}

void RejectsMissingFile() {
    Check(!toolscreen::pe::ExportsSymbol(L"Z:\\definitely\\missing\\file.dll", "GetLastError"), "missing file");
}

void RejectsNonPeFile() {
    const auto path = TempFile(".txt");
    std::ofstream(path, std::ios::binary) << "this is not a portable executable";
    Check(!toolscreen::pe::ExportsSymbol(path.wstring(), "GetLastError"), "text file");
    std::filesystem::remove(path);
}

void RejectsEmptyFile() {
    const auto path = TempFile(".empty");
    std::ofstream(path, std::ios::binary).close();
    Check(!toolscreen::pe::ExportsSymbol(path.wstring(), "GetLastError"), "empty file");
    std::filesystem::remove(path);
}

void RejectsTruncatedPe() {
    std::ifstream in(std::filesystem::path(SystemDll()), std::ios::binary);
    std::vector<char> head(5000);
    in.read(head.data(), static_cast<std::streamsize>(head.size()));
    const auto path = TempFile(".trunc");
    std::ofstream(path, std::ios::binary).write(head.data(), in.gcount());
    Check(!toolscreen::pe::ExportsSymbol(path.wstring(), "GetLastError"), "truncated PE");
    std::filesystem::remove(path);
}

void RejectsCorruptExportTable() {
    std::ifstream in(std::filesystem::path(SystemDll()), std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(bytes.data() + dos->e_lfanew);
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress = 0xFFFFFFF0u;
    const auto path = TempFile(".corrupt");
    std::ofstream(path, std::ios::binary).write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    Check(!toolscreen::pe::ExportsSymbol(path.wstring(), "GetLastError"), "export table pointing outside the file");
    std::filesystem::remove(path);
}

struct TestCase {
    const char* name;
    void (*run)();
};

}  // namespace

int main(int argc, char** argv) {
    const TestCase cases[] = {
        {"finds_exported_symbol", &FindsExportedSymbol},
        {"rejects_unexported_symbol", &RejectsUnexportedSymbol},
        {"requires_exact_name", &RequiresExactName},
        {"rejects_missing_file", &RejectsMissingFile},
        {"rejects_non_pe_file", &RejectsNonPeFile},
        {"rejects_empty_file", &RejectsEmptyFile},
        {"rejects_truncated_pe", &RejectsTruncatedPe},
        {"rejects_corrupt_export_table", &RejectsCorruptExportTable},
    };
    if (argc != 3 || std::strcmp(argv[1], "--run") != 0) {
        std::cerr << "Usage: " << argv[0] << " --run <case>\n";
        return 2;
    }
    for (const auto& test : cases) {
        if (std::strcmp(argv[2], test.name) == 0) {
            std::cout << "RUN " << test.name << "\n";
            test.run();
            if (g_failures != 0) {
                std::cout << "FAIL " << test.name << "\n";
                return 1;
            }
            std::cout << "PASS " << test.name << "\n";
            return 0;
        }
    }
    std::cerr << "Unknown test case: " << argv[2] << "\n";
    return 2;
}
