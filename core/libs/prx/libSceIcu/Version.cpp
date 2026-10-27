#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstdint>

namespace {

constexpr std::uint16_t VersionLength = 4;
constexpr std::int32_t VersionStringLength = 20;
constexpr char VersionDelimiter = '.';

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
    std::uint16_t part = 0;
    if (text) {
        for (;;) {
            char* end = nullptr;
            version[part] = static_cast<std::uint8_t>(strtoul_nid_postfix(text, &end, 10));
            if (end == text || ++part == VersionLength || *end != VersionDelimiter) break;
            text = end + 1;
        }
    }
    while (part < VersionLength) version[part++] = 0;
}

void APS5_VABI u_versionFromUString_nid_postfix(std::uint8_t* version, const char16_t* text) {
    if (!version || !text) return;
    char chars[VersionStringLength + 1];
    std::int32_t length = 0;
    while (length < VersionStringLength && text[length]) ++length;
    u_UCharsToChars_nid_postfix(text, chars, length);
    chars[length] = 0;
    u_versionFromString_nid_postfix(version, chars);
}

void APS5_VABI u_versionToString_nid_postfix(const std::uint8_t* version, char* text) {
    if (!text) return;
    if (!version) {
        text[0] = 0;
        return;
    }
    std::uint16_t count = VersionLength;
    while (count > 0 && version[count - 1] == 0) --count;
    if (count <= 1) count = 2;
    text = WriteField(text, version[0]);
    for (std::uint16_t part = 1; part < count; ++part) {
        *text++ = VersionDelimiter;
        text = WriteField(text, version[part]);
    }
    *text = 0;
}

}
