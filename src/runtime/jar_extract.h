#pragma once

#include "third_party/stb_image.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <string>

namespace toolscreen::auto_updater {

inline bool ExtractZipEntry(const std::string& zip, const std::string& entryName, size_t maxUncompressedBytes, std::string& out) {
    constexpr uint32_t kEndOfCentralDirSignature = 0x06054b50;
    constexpr uint32_t kCentralEntrySignature = 0x02014b50;
    constexpr uint32_t kLocalHeaderSignature = 0x04034b50;
    constexpr size_t kEndOfCentralDirSize = 22;
    constexpr size_t kMaxCommentSize = 65535;
    constexpr size_t kCentralEntryFixedSize = 46;
    constexpr size_t kLocalHeaderFixedSize = 30;
    constexpr uint16_t kMethodStored = 0;
    constexpr uint16_t kMethodDeflate = 8;
    constexpr uint16_t kZip64Marker16 = 0xFFFF;
    constexpr uint32_t kZip64Marker32 = 0xFFFFFFFF;

    const auto read16 = [&](size_t offset) {
        uint16_t value = 0;
        std::memcpy(&value, zip.data() + offset, sizeof(value));
        return value;
    };
    const auto read32 = [&](size_t offset) {
        uint32_t value = 0;
        std::memcpy(&value, zip.data() + offset, sizeof(value));
        return value;
    };

    if (zip.size() < kEndOfCentralDirSize) return false;

    const size_t lastEocdStart = zip.size() - kEndOfCentralDirSize;
    const size_t firstEocdStart = lastEocdStart > kMaxCommentSize ? lastEocdStart - kMaxCommentSize : 0;
    size_t eocd = std::string::npos;
    for (size_t i = lastEocdStart;; --i) {
        if (read32(i) == kEndOfCentralDirSignature) { eocd = i; break; }
        if (i == firstEocdStart) break;
    }
    if (eocd == std::string::npos) return false;

    const uint16_t entryCount = read16(eocd + 10);
    const uint32_t centralDirSize = read32(eocd + 12);
    const uint32_t centralDirOffset = read32(eocd + 16);
    if (entryCount == kZip64Marker16 || centralDirSize == kZip64Marker32 || centralDirOffset == kZip64Marker32) return false;
    if (static_cast<uint64_t>(centralDirOffset) + centralDirSize > zip.size()) return false;

    const size_t centralDirEnd = static_cast<size_t>(centralDirOffset) + centralDirSize;
    size_t cursor = centralDirOffset;
    for (uint16_t index = 0; index < entryCount; ++index) {
        if (cursor + kCentralEntryFixedSize > centralDirEnd) return false;
        if (read32(cursor) != kCentralEntrySignature) return false;

        const uint16_t method = read16(cursor + 10);
        const uint32_t compressedSize = read32(cursor + 20);
        const uint32_t uncompressedSize = read32(cursor + 24);
        const uint16_t nameLength = read16(cursor + 28);
        const uint16_t extraLength = read16(cursor + 30);
        const uint16_t commentLength = read16(cursor + 32);
        const uint32_t localHeaderOffset = read32(cursor + 42);
        const size_t entryEnd = cursor + kCentralEntryFixedSize + nameLength + extraLength + commentLength;
        if (entryEnd > centralDirEnd) return false;

        if (zip.compare(cursor + kCentralEntryFixedSize, nameLength, entryName) == 0 && nameLength == entryName.size()) {
            if (uncompressedSize > maxUncompressedBytes || uncompressedSize > INT_MAX || compressedSize > INT_MAX) return false;
            if (static_cast<uint64_t>(localHeaderOffset) + kLocalHeaderFixedSize > zip.size()) return false;
            if (read32(localHeaderOffset) != kLocalHeaderSignature) return false;

            const size_t dataStart = static_cast<size_t>(localHeaderOffset) + kLocalHeaderFixedSize +
                                     read16(localHeaderOffset + 26) + read16(localHeaderOffset + 28);
            if (dataStart + compressedSize > zip.size()) return false;

            if (method == kMethodStored) {
                if (compressedSize != uncompressedSize) return false;
                out.assign(zip, dataStart, compressedSize);
                return true;
            }
            if (method != kMethodDeflate) return false;

            out.assign(uncompressedSize, '\0');
            const int decoded = stbi_zlib_decode_noheader_buffer(out.data(), static_cast<int>(uncompressedSize),
                                                                 zip.data() + dataStart, static_cast<int>(compressedSize));
            return decoded == static_cast<int>(uncompressedSize);
        }
        cursor = entryEnd;
    }
    return false;
}

} // namespace toolscreen::auto_updater
