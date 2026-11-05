#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <windows.h>

namespace toolscreen::pe {

inline bool ExportsSymbol(const std::wstring& filePath, const char* symbolName) {
    std::ifstream in(std::filesystem::path(filePath), std::ios::binary);
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto base = reinterpret_cast<const uint8_t*>(bytes.data());
    const size_t size = bytes.size();
    const auto inRange = [&](size_t offset, size_t length) { return offset <= size && length <= size - offset; };

    if (!inRange(0, sizeof(IMAGE_DOS_HEADER))) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0) return false;
    const size_t ntOffset = static_cast<size_t>(dos->e_lfanew);
    if (!inRange(ntOffset, sizeof(IMAGE_NT_HEADERS64))) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + ntOffset);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
    if (nt->OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_EXPORT) return false;

    const size_t sectionsOffset = ntOffset + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt->FileHeader.SizeOfOptionalHeader;
    const size_t sectionCount = nt->FileHeader.NumberOfSections;
    if (!inRange(sectionsOffset, sectionCount * sizeof(IMAGE_SECTION_HEADER))) return false;
    const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(base + sectionsOffset);

    const auto rvaToOffset = [&](DWORD rva, size_t& offset) {
        for (size_t i = 0; i < sectionCount; ++i) {
            const DWORD start = sections[i].VirtualAddress;
            const DWORD span = (std::max)(sections[i].Misc.VirtualSize, sections[i].SizeOfRawData);
            if (rva >= start && rva - start < span) {
                offset = static_cast<size_t>(sections[i].PointerToRawData) + (rva - start);
                return true;
            }
        }
        return false;
    };

    const IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    size_t exportOffset = 0;
    if (dir.VirtualAddress == 0 || !rvaToOffset(dir.VirtualAddress, exportOffset)) return false;
    if (!inRange(exportOffset, sizeof(IMAGE_EXPORT_DIRECTORY))) return false;
    const auto* exports = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(base + exportOffset);

    size_t namesOffset = 0;
    if (!rvaToOffset(exports->AddressOfNames, namesOffset)) return false;
    if (!inRange(namesOffset, static_cast<size_t>(exports->NumberOfNames) * sizeof(DWORD))) return false;
    const auto* nameRvas = reinterpret_cast<const DWORD*>(base + namesOffset);

    const std::string wanted = symbolName;
    for (DWORD i = 0; i < exports->NumberOfNames; ++i) {
        size_t nameOffset = 0;
        if (!rvaToOffset(nameRvas[i], nameOffset) || !inRange(nameOffset, wanted.size() + 1)) continue;
        if (std::memcmp(base + nameOffset, wanted.c_str(), wanted.size() + 1) == 0) return true;
    }
    return false;
}

} // namespace toolscreen::pe
