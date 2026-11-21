#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>

namespace {

constexpr std::uint16_t VersionLength = 4;
constexpr std::int32_t VersionStringLength = 20;
constexpr char VersionDelimiter = '.';
constexpr std::uint8_t UnicodeVersion[VersionLength] = {17, 0, 0, 0};
constexpr const char* IcuVersion = "78.1";
constexpr std::size_t VersionTextCapacity = 256;

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

template <typename Unit>
bool CopyText(const Unit* text, Unit* copy, std::size_t capacity, std::size_t& length) {
    length = 0;
    while (length < capacity) {
        const auto address = reinterpret_cast<std::uintptr_t>(text + length);
        const std::size_t room = (std::max)(std::size_t{1}, (0x1000 - address % 0x1000) / sizeof(Unit));
        const std::size_t count = (std::min)(capacity - length, room);
        if (!CopyIn(copy + length, text + length, count * sizeof(Unit))) return false;
        for (std::size_t index = 0; index < count; ++index) {
            if (copy[length + index] == 0) {
                length += index;
                return true;
            }
        }
        length += count;
    }
    return true;
}

char* WriteField(char* text, std::uint8_t field) {
    if (field >= 100) {
        *text++ = static_cast<char>('0' + field / 100);
        field %= 100;
    }
    if (field >= 10) {
        *text++ = static_cast<char>('0' + field / 10);
        field %= 10;
    }
    *text++ = static_cast<char>('0' + field);
    return text;
}

}

extern "C" {

void APS5_VABI u_versionFromString_nid_postfix(std::uint8_t* version, const char* text) {
    if (!version) return;
    std::uint8_t parsed[VersionLength] = {};
    char copy[VersionTextCapacity + 1];
    std::size_t length = 0;
    if (text && CopyText(text, copy, VersionTextCapacity, length)) {
        copy[length] = 0;
        const char* cursor = copy;
        for (std::uint16_t part = 0;;) {
            char* end = nullptr;
            parsed[part] = static_cast<std::uint8_t>(strtoul_nid_postfix(cursor, &end, 10));
            if (end == cursor || ++part == VersionLength || *end != VersionDelimiter) break;
            cursor = end + 1;
        }
    }
    CopyOut(version, parsed, sizeof(parsed));
}

void APS5_VABI u_versionFromUString_nid_postfix(std::uint8_t* version, const char16_t* text) {
    if (!version || !text) return;
    char16_t units[VersionStringLength];
    std::size_t length = 0;
    if (!CopyText(text, units, VersionStringLength, length)) return;
    char chars[VersionStringLength + 1];
    u_UCharsToChars_nid_postfix(units, chars, static_cast<std::int32_t>(length));
    chars[length] = 0;
    u_versionFromString_nid_postfix(version, chars);
}

void APS5_VABI u_versionToString_nid_postfix(const std::uint8_t* version, char* text) {
    if (!text) return;
    std::uint8_t parts[VersionLength];
    if (!version || !CopyIn(parts, version, sizeof(parts))) {
        const char empty = 0;
        CopyOut(text, &empty, sizeof(empty));
        return;
    }
    std::uint16_t count = VersionLength;
    while (count > 0 && parts[count - 1] == 0) --count;
    if (count <= 1) count = 2;
    char formatted[VersionStringLength];
    char* cursor = WriteField(formatted, parts[0]);
    for (std::uint16_t part = 1; part < count; ++part) {
        *cursor++ = VersionDelimiter;
        cursor = WriteField(cursor, parts[part]);
    }
    *cursor++ = 0;
    CopyOut(text, formatted, static_cast<std::size_t>(cursor - formatted));
}

void APS5_VABI u_getUnicodeVersion_nid_postfix(std::uint8_t* version) {
    if (!version) return;
    CopyOut(version, UnicodeVersion, sizeof(UnicodeVersion));
}

void APS5_VABI u_getVersion_nid_postfix(std::uint8_t* version) {
    u_versionFromString_nid_postfix(version, IcuVersion);
}

}
