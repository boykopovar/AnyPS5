// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace {

struct Range {
    std::int32_t first;
    std::int32_t last;
};

constexpr std::array Syntax{
    Range{0x21, 0x2f}, Range{0x3a, 0x40}, Range{0x5b, 0x5e}, Range{0x60, 0x60}, Range{0x7b, 0x7e},
    Range{0xa1, 0xa7}, Range{0xa9, 0xa9}, Range{0xab, 0xac}, Range{0xae, 0xae}, Range{0xb0, 0xb1},
    Range{0xb6, 0xb6}, Range{0xbb, 0xbb}, Range{0xbf, 0xbf}, Range{0xd7, 0xd7}, Range{0xf7, 0xf7},
    Range{0x2010, 0x2027}, Range{0x2030, 0x203e}, Range{0x2041, 0x2053}, Range{0x2055, 0x205e},
    Range{0x2190, 0x245f}, Range{0x2500, 0x2775}, Range{0x2794, 0x2bff}, Range{0x2e00, 0x2e7f},
    Range{0x3001, 0x3003}, Range{0x3008, 0x3020}, Range{0x3030, 0x3030}, Range{0xfd3e, 0xfd3f}, Range{0xfe45, 0xfe46}
};

bool IsWhiteSpace(std::int32_t character) {
    return (character >= 9 && character <= 13) || character == 0x20 || character == 0x85 ||
        character == 0x200e || character == 0x200f || character == 0x2028 || character == 0x2029;
}

bool IsSyntax(std::int32_t character) {
    return std::any_of(Syntax.begin(), Syntax.end(), [character](const auto& range) {
        return character >= range.first && character <= range.last;
    });
}

char16_t Read(const char16_t* text) {
    if (!text) throw std::invalid_argument("PatternProps: null input");
    return *text;
}

}

extern "C" {

std::int8_t APS5_VABI _ZN6icu_6812PatternProps8isSyntaxEi_nid_postfix(std::int32_t character) {
    return IsSyntax(character);
}

std::int8_t APS5_VABI _ZN6icu_6812PatternProps12isWhiteSpaceEi_nid_postfix(std::int32_t character) {
    return IsWhiteSpace(character);
}

std::int8_t APS5_VABI _ZN6icu_6812PatternProps20isSyntaxOrWhiteSpaceEi_nid_postfix(std::int32_t character) {
    return IsSyntax(character) || IsWhiteSpace(character);
}

std::int8_t APS5_VABI _ZN6icu_6812PatternProps12isIdentifierEPKDsi_nid_postfix(const char16_t* text, std::int32_t length) {
    if (length <= 0) return 0;
    if (!text) throw std::invalid_argument("PatternProps: null input");
    for (std::int32_t index = 0; index < length; ++index)
        if (IsSyntax(text[index]) || IsWhiteSpace(text[index])) return 0;
    return 1;
}

const char16_t* APS5_VABI _ZN6icu_6812PatternProps14skipWhiteSpaceEPKDsi_nid_postfix(const char16_t* text, std::int32_t length) {
    while (length > 0 && IsWhiteSpace(Read(text))) {
        ++text;
        --length;
    }
    return text;
}

const char16_t* APS5_VABI _ZN6icu_6812PatternProps14skipIdentifierEPKDsi_nid_postfix(const char16_t* text, std::int32_t length) {
    while (length > 0) {
        const auto unit = Read(text);
        if (IsSyntax(unit) || IsWhiteSpace(unit)) break;
        ++text;
        --length;
    }
    return text;
}

const char16_t* APS5_VABI _ZN6icu_6812PatternProps14trimWhiteSpaceEPKDsRi_nid_postfix(const char16_t* text, std::int32_t* length) {
    if (!length) throw std::invalid_argument("PatternProps: null length");
    if (*length <= 0) return text;
    if (!text) throw std::invalid_argument("PatternProps: null input");
    if (!IsWhiteSpace(text[0]) && !IsWhiteSpace(text[*length - 1])) return text;
    std::int32_t start = 0;
    auto limit = *length;
    while (start < limit && IsWhiteSpace(text[start])) ++start;
    if (start < limit) while (IsWhiteSpace(text[limit - 1])) --limit;
    *length = limit - start;
    return text + start;
}

std::int32_t APS5_VABI _ZN6icu_6812PatternProps14skipWhiteSpaceERKNS_13UnicodeStringEi_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t start) {
    const auto length = _ZNK6icu_6813UnicodeString9getLengthEv_nid_postfix(text);
    while (start < length && IsWhiteSpace(_ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(text, start))) ++start;
    return start;
}

}
