#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

int main() {
    const auto check = [](bool value) { if (!value) std::abort(); };
    int syntaxCount = 0;
    int whiteCount = 0;
    std::uint64_t syntaxHash = 14695981039346656037ull;
    std::uint64_t whiteHash = 14695981039346656037ull;
    for (std::int32_t point = -1; point <= 0x110000; ++point) {
        const auto syntax = _ZN6icu_6812PatternProps8isSyntaxEi_nid_postfix(point);
        const auto white = _ZN6icu_6812PatternProps12isWhiteSpaceEi_nid_postfix(point);
        check(_ZN6icu_6812PatternProps20isSyntaxOrWhiteSpaceEi_nid_postfix(point) == (syntax || white));
        syntaxCount += syntax;
        whiteCount += white;
        syntaxHash = (syntaxHash ^ syntax) * 1099511628211ull;
        whiteHash = (whiteHash ^ white) * 1099511628211ull;
    }
    check(syntaxCount == 2760 && whiteCount == 11);
    check(syntaxHash == 0x5c63c889094606bfull && whiteHash == 0xf0b3606f27bacab6ull);
    for (auto point : {9, 10, 11, 12, 13, 32, 0x85, 0x200e, 0x200f, 0x2028, 0x2029})
        check(_ZN6icu_6812PatternProps12isWhiteSpaceEi_nid_postfix(point));
    for (auto point : {0xa0, 0x1680, 0x2000, 0x200b, 0x202f, 0x205f, 0x3000, -1, std::numeric_limits<std::int32_t>::max()})
        check(!_ZN6icu_6812PatternProps12isWhiteSpaceEi_nid_postfix(point));
    for (auto point : {u'!', u'$', u'~', char16_t{0x2041}, char16_t{0x2775}, char16_t{0x2794}, char16_t{0xfd3e}, char16_t{0xfe46}})
        check(_ZN6icu_6812PatternProps8isSyntaxEi_nid_postfix(point));
    for (auto point : {u'_', u'0', u'A', char16_t{0x2054}, char16_t{0x2776}, char16_t{0x2793}, char16_t{0x3031}, char16_t{0xfd40}})
        check(!_ZN6icu_6812PatternProps8isSyntaxEi_nid_postfix(point));
    const char16_t text[]{u' ', 0x200e, u'A', u'_', 0xd800, u' ', 0x2029};
    check(_ZN6icu_6812PatternProps14skipWhiteSpaceEPKDsi_nid_postfix(text, 7) == text + 2);
    check(_ZN6icu_6812PatternProps14skipWhiteSpaceEPKDsi_nid_postfix(text, 1) == text + 1);
    check(_ZN6icu_6812PatternProps14skipIdentifierEPKDsi_nid_postfix(text + 2, 5) == text + 5);
    check(_ZN6icu_6812PatternProps12isIdentifierEPKDsi_nid_postfix(text + 2, 3));
    check(!_ZN6icu_6812PatternProps12isIdentifierEPKDsi_nid_postfix(text, 7));
    check(!_ZN6icu_6812PatternProps12isIdentifierEPKDsi_nid_postfix(nullptr, 0));
    check(_ZN6icu_6812PatternProps14skipWhiteSpaceEPKDsi_nid_postfix(nullptr, -1) == nullptr);
    check(_ZN6icu_6812PatternProps14skipIdentifierEPKDsi_nid_postfix(nullptr, 0) == nullptr);
    std::int32_t length = 7;
    check(_ZN6icu_6812PatternProps14trimWhiteSpaceEPKDsRi_nid_postfix(text, &length) == text + 2 && length == 3);
    length = 2;
    check(_ZN6icu_6812PatternProps14trimWhiteSpaceEPKDsRi_nid_postfix(text, &length) == text + 2 && length == 0);
    length = -1;
    check(_ZN6icu_6812PatternProps14trimWhiteSpaceEPKDsRi_nid_postfix(nullptr, &length) == nullptr && length == -1);
    Icu::Abi::UnicodeString string{};
    string.storage.fields = {static_cast<std::int16_t>((7 << 5) | 8), 0, 7, const_cast<char16_t*>(text)};
    check(_ZN6icu_6812PatternProps14skipWhiteSpaceERKNS_13UnicodeStringEi_nid_postfix(&string, 0) == 2);
    check(_ZN6icu_6812PatternProps14skipWhiteSpaceERKNS_13UnicodeStringEi_nid_postfix(&string, -1) == -1);
    check(_ZN6icu_6812PatternProps14skipWhiteSpaceERKNS_13UnicodeStringEi_nid_postfix(&string, 99) == 99);
    bool threw = false;
    try { _ZN6icu_6812PatternProps14skipWhiteSpaceEPKDsi_nid_postfix(nullptr, 1); }
    catch (const std::invalid_argument&) { threw = true; }
    check(threw);
    std::puts("ICU PatternProps tests passed");
}
