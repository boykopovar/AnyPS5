#include "prx/libSceNKWebKit/src/Types.hpp"
#include <cstring>

namespace NKWebKit {

std::optional<std::u16string> DecodeUtf8(const char* text, std::size_t length) {
    if (!text) return std::nullopt;
    std::u16string result;
    result.reserve(length);
    const auto* cursor = reinterpret_cast<const unsigned char*>(text);
    const auto* end = cursor + length;
    while (cursor < end) {
        std::uint32_t codepoint = *cursor++;
        unsigned following = 0;
        std::uint32_t minimum = 0;
        if (codepoint < 0x80) {
        } else if (codepoint >= 0xC2 && codepoint <= 0xDF) {
            codepoint &= 0x1F;
            following = 1;
            minimum = 0x80;
        } else if (codepoint >= 0xE0 && codepoint <= 0xEF) {
            codepoint &= 0x0F;
            following = 2;
            minimum = 0x800;
        } else if (codepoint >= 0xF0 && codepoint <= 0xF4) {
            codepoint &= 0x07;
            following = 3;
            minimum = 0x10000;
        } else return std::nullopt;
        if (static_cast<std::size_t>(end - cursor) < following) return std::nullopt;
        for (; following > 0; --following) {
            if ((*cursor & 0xC0) != 0x80) return std::nullopt;
            codepoint = codepoint << 6 | (*cursor++ & 0x3F);
        }
        if (codepoint < minimum || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) return std::nullopt;
        if (codepoint > 0xFFFF) {
            const auto supplementary = codepoint - 0x10000;
            result.push_back(static_cast<char16_t>(0xD800 + (supplementary >> 10)));
            result.push_back(static_cast<char16_t>(0xDC00 + (supplementary & 0x3FF)));
        } else result.push_back(static_cast<char16_t>(codepoint));
    }
    return result;
}

std::optional<std::u16string> DecodeUtf8(const char* text) {
    if (!text) return std::nullopt;
    return DecodeUtf8(text, std::strlen(text));
}

std::string EncodeUtf8(std::u16string_view text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        std::uint32_t codepoint = text[index];
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF && index + 1 < text.size() && text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF) {
            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (text[++index] - 0xDC00);
        }
        if (codepoint < 0x80) result.push_back(static_cast<char>(codepoint));
        else if (codepoint < 0x800) {
            result.push_back(static_cast<char>(0xC0 | codepoint >> 6));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint < 0x10000) {
            result.push_back(static_cast<char>(0xE0 | codepoint >> 12));
            result.push_back(static_cast<char>(0x80 | (codepoint >> 6 & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | codepoint >> 18));
            result.push_back(static_cast<char>(0x80 | (codepoint >> 12 & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint >> 6 & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    }
    return result;
}

std::size_t ConvertUtf16ToUtf8(std::u16string_view text, char* buffer, std::size_t bufferSize, bool strict) {
    if (!buffer || bufferSize == 0) return 0;
    char* target = buffer;
    char* targetEnd = buffer + bufferSize - 1;
    for (std::size_t index = 0; index < text.size(); ) {
        std::uint32_t codepoint = text[index++];
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            if (index < text.size() && text[index] >= 0xDC00 && text[index] <= 0xDFFF) {
                codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (text[index++] - 0xDC00);
            } else {
                if (strict) return 0;
                codepoint = 0xFFFD;
            }
        } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
            if (strict) return 0;
            codepoint = 0xFFFD;
        }
        std::size_t needed = (codepoint < 0x80) ? 1 : (codepoint < 0x800) ? 2 : (codepoint < 0x10000) ? 3 : 4;
        if (static_cast<std::size_t>(targetEnd - target) < needed) break;
        if (needed == 1) {
            *target++ = static_cast<char>(codepoint);
        } else if (needed == 2) {
            *target++ = static_cast<char>(0xC0 | (codepoint >> 6));
            *target++ = static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (needed == 3) {
            *target++ = static_cast<char>(0xE0 | (codepoint >> 12));
            *target++ = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            *target++ = static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            *target++ = static_cast<char>(0xF0 | (codepoint >> 18));
            *target++ = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            *target++ = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            *target++ = static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }
    *target++ = '\0';
    return static_cast<std::size_t>(target - buffer);
}

}  // namespace NKWebKit
