// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2010-2015, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1997-2016, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 2009-2013, International Business Machines Corporation and others. All Rights Reserved.
// Copyright 2004 and onwards Google Inc.
// Copyright (c) 2001-2011, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
using namespace Icu::Abi;
constexpr char16_t Digits[] = u"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
template<typename TValue> TValue& Require(TValue* value) {
    if (!value) throw std::invalid_argument("ICU string utility: null pointer");
    return *value;
}
void Unit(UnicodeString* text, char16_t unit) { Require(text).Append(&unit, 0, 1); }
bool White(std::int32_t point) { return _ZN6icu_6812PatternProps12isWhiteSpaceEi_nid_postfix(point) != 0; }
std::int32_t Advance(std::int32_t index, std::int32_t point) {
    return CheckedInt_nid_no_patch(static_cast<std::int64_t>(index) + (static_cast<std::uint32_t>(point) <= 0xffff ? 1 : 2));
}
}

extern "C" {
Icu::Abi::UnicodeString* APS5_VABI _ZN6icu_6811ICU_Utility12appendNumberERNS_13UnicodeStringEiii_nid_postfix(Icu::Abi::UnicodeString* result, std::int32_t number, std::int32_t radix, std::int32_t minimum) {
    if (radix < 2 || radix > 36) { Unit(result, u'?'); return result; }
    if (number == INT32_MIN) throw std::invalid_argument("ICU number: signed negation overflow");
    if (number < 0) { number = -number; Unit(result, u'-'); }
    std::int32_t power = 1;
    for (auto remaining = number; remaining >= radix; remaining /= radix) {
        power = CheckedInt_nid_no_patch(static_cast<std::int64_t>(power) * radix);
        minimum = CheckedInt_nid_no_patch(static_cast<std::int64_t>(minimum) - 1);
    }
    for (;;) {
        minimum = CheckedInt_nid_no_patch(static_cast<std::int64_t>(minimum) - 1);
        if (minimum <= 0) break;
        Unit(result, u'0');
    }
    for (; power > 0; power /= radix) {
        const auto digit = number / power;
        Unit(result, Digits[digit]);
        number -= digit * power;
    }
    return result;
}

std::int8_t APS5_VABI _ZN6icu_6811ICU_Utility17escapeUnprintableERNS_13UnicodeStringEi_nid_postfix(Icu::Abi::UnicodeString* result, std::int32_t point) {
    if (point >= 0x20 && point <= 0x7e) return 0;
    Unit(result, u'\\');
    Unit(result, point & ~0xffff ? u'U' : u'u');
    for (int shift = point & ~0xffff ? 28 : 12; shift >= 0; shift -= 4)
        Unit(result, Digits[(static_cast<std::uint32_t>(point) >> shift) & 15]);
    return 1;
}

std::int32_t APS5_VABI _ZN6icu_6811ICU_Utility14skipWhitespaceERKNS_13UnicodeStringERia_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t* position, std::int8_t advance) {
    const auto& string = Require(text);
    auto index = Require(position);
    const auto* buffer = string.Buffer();
    Require(buffer);
    if (index < 0) throw std::invalid_argument("ICU whitespace: negative index");
    while (index < string.Length() && White(buffer[index])) ++index;
    if (advance) *position = index;
    return index;
}

std::int8_t APS5_VABI _ZN6icu_6811ICU_Utility9parseCharERKNS_13UnicodeStringERiDs_nid_postfix(const Icu::Abi::UnicodeString* text, std::int32_t* position, char16_t unit) {
    const auto start = Require(position);
    _ZN6icu_6811ICU_Utility14skipWhitespaceERKNS_13UnicodeStringERia_nid_postfix(text, position, 1);
    if (*position == Require(text).Length() || _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(text, *position) != unit) {
        *position = start;
        return 0;
    }
    *position = CheckedInt_nid_no_patch(static_cast<std::int64_t>(*position) + 1);
    return 1;
}

std::int32_t APS5_VABI _ZN6icu_6811ICU_Utility12parsePatternERKNS_13UnicodeStringERKNS_11ReplaceableEii_nid_postfix(const Icu::Abi::UnicodeString* pattern, const void* text, std::int32_t index, std::int32_t limit) {
    const auto length = Require(pattern).Length();
    if (!length) return index;
    std::int32_t offset = 0;
    auto expected = _ZNK6icu_6813UnicodeString8char32AtEi_nid_postfix(pattern, offset);
    while (index < limit) {
        const auto actual = Virtual_nid_no_patch<std::int32_t (APS5_VABI *)(const void*, std::int32_t)>(text, 10)(text, index);
        if (expected == 126) {
            if (White(actual)) { index = Advance(index, actual); continue; }
            if (++offset == length) return index;
        } else if (actual == expected) {
            index = Advance(index, actual);
            offset = Advance(offset, expected);
            if (offset == length) return index;
        } else return -1;
        expected = _ZNK6icu_6813UnicodeString8char32AtEi_nid_postfix(pattern, offset);
    }
    return -1;
}

char* APS5_VABI _ZNK6icu_6810CharString9cloneDataER10UErrorCode_nid_postfix(const Icu::Abi::CharString* self, std::int32_t* status) {
    if (Require(status) > 0) return nullptr;
    const auto& object = Require(self);
    const auto count = CheckedInt_nid_no_patch(static_cast<std::int64_t>(object.length) + 1);
    if (count < 0) throw std::invalid_argument("ICU clone data: negative length");
    auto* result = static_cast<char*>(Allocate_nid_no_patch(count));
    if (!result) { *status = 7; return nullptr; }
    if (count) { Require(object.buffer.pointer); std::memcpy(result, object.buffer.pointer, count); }
    return result;
}

char* APS5_VABI _ZN6icu_6815MaybeStackArrayIcLi40EE13orphanOrCloneEiRi_nid_postfix(Icu::Abi::MaybeStackArray40* self, std::int32_t length, std::int32_t* capacity) {
    auto& object = Require(self);
    auto* result = object.pointer;
    if (!object.needToRelease) {
        if (length <= 0) return nullptr;
        length = std::min(length, object.capacity);
        if (length < 0) throw std::invalid_argument("ICU orphan: negative capacity");
        result = static_cast<char*>(Allocate_nid_no_patch(length));
        if (!result) return nullptr;
        if (length) { Require(object.pointer); std::memcpy(result, object.pointer, length); }
    }
    Require(capacity) = length;
    object.pointer = object.stack;
    object.capacity = 40;
    object.needToRelease = 0;
    return result;
}

std::int32_t APS5_VABI _ZN6icu_6811StringPiece4findES0_i_nid_postfix(const Icu::Abi::StringPiece* self, Icu::Abi::StringPiece needle, std::int32_t offset) {
    const auto& object = Require(self);
    if (!object.length && !needle.length) return 0;
    for (auto index = offset; index < object.length; index = CheckedInt_nid_no_patch(static_cast<std::int64_t>(index) + 1)) {
        std::int32_t j = 0;
        for (; j < needle.length; ++j) {
            if (index < 0) throw std::invalid_argument("ICU find: negative index");
            Require(object.pointer);
            Require(needle.pointer);
            if (object.pointer[index] != needle.pointer[j]) break;
            index = CheckedInt_nid_no_patch(static_cast<std::int64_t>(index) + 1);
        }
        if (j == needle.length || needle.length < 0) return CheckedInt_nid_no_patch(static_cast<std::int64_t>(index) - j);
        index = CheckedInt_nid_no_patch(static_cast<std::int64_t>(index) - j);
    }
    return -1;
}

void APS5_VABI _ZN6icu_6811ICU_Utility12appendToRuleERNS_13UnicodeStringEiaaS2__nid_postfix(Icu::Abi::UnicodeString* rulePtr, std::int32_t c, std::int8_t isLiteral, std::int8_t escapeUnprintable, Icu::Abi::UnicodeString* quoteBufPtr) {
    auto& rule = Require(rulePtr);
    auto& quoteBuf = Require(quoteBufPtr);
    constexpr char16_t APOSTROPHE = 0x0027;
    constexpr char16_t BACKSLASH = 0x005C;
    constexpr char16_t SPACE = 0x0020;
    if (isLiteral || (escapeUnprintable && _ZN6icu_6811ICU_Utility13isUnprintableEi_nid_postfix(c))) {
        if (quoteBuf.Length() > 0) {
            while (quoteBuf.Length() >= 2 &&
                   quoteBuf.Buffer()[0] == APOSTROPHE &&
                   quoteBuf.Buffer()[1] == APOSTROPHE) {
                Unit(&rule, BACKSLASH);
                Unit(&rule, APOSTROPHE);
                quoteBuf.Replace(0, 2, nullptr, 0, 0);
            }
            std::int32_t trailingCount = 0;
            while (quoteBuf.Length() >= 2 &&
                   quoteBuf.Buffer()[quoteBuf.Length() - 2] == APOSTROPHE &&
                   quoteBuf.Buffer()[quoteBuf.Length() - 1] == APOSTROPHE) {
                quoteBuf.Replace(quoteBuf.Length() - 2, 2, nullptr, 0, 0);
                ++trailingCount;
            }
            if (quoteBuf.Length() > 0) {
                Unit(&rule, APOSTROPHE);
                rule.Append(quoteBuf.Buffer(), 0, quoteBuf.Length());
                Unit(&rule, APOSTROPHE);
                quoteBuf.Replace(0, quoteBuf.Length(), nullptr, 0, 0);
            }
            while (trailingCount-- > 0) {
                Unit(&rule, BACKSLASH);
                Unit(&rule, APOSTROPHE);
            }
        }
        if (c != static_cast<std::int32_t>(-1)) {
            if (c == SPACE) {
                const auto len = rule.Length();
                if (len > 0 && rule.Buffer()[len - 1] != c) {
                    _ZN6icu_6813UnicodeString6appendEi_nid_postfix(&rule, c);
                }
            } else if (!escapeUnprintable || !_ZN6icu_6811ICU_Utility17escapeUnprintableERNS_13UnicodeStringEi_nid_postfix(&rule, c)) {
                _ZN6icu_6813UnicodeString6appendEi_nid_postfix(&rule, c);
            }
        }
    } else if (quoteBuf.Length() == 0 && (c == APOSTROPHE || c == BACKSLASH)) {
        Unit(&rule, BACKSLASH);
        Unit(&rule, static_cast<char16_t>(c));
    } else if (quoteBuf.Length() > 0 ||
               (c >= 0x0021 && c <= 0x007e &&
                !((c >= 0x0030 && c <= 0x0039) ||
                  (c >= 0x0041 && c <= 0x005a) ||
                  (c >= 0x0061 && c <= 0x007a))) ||
               White(c)) {
        _ZN6icu_6813UnicodeString6appendEi_nid_postfix(&quoteBuf, c);
        if (c == APOSTROPHE) {
            Unit(&quoteBuf, APOSTROPHE);
        }
    } else {
        _ZN6icu_6813UnicodeString6appendEi_nid_postfix(&rule, c);
    }
}

void APS5_VABI _ZN6icu_6811ICU_Utility12appendToRuleERNS_13UnicodeStringERKS1_aaS2__nid_postfix(Icu::Abi::UnicodeString* rule, const Icu::Abi::UnicodeString* text, std::int8_t isLiteral, std::int8_t escapeUnprintable, Icu::Abi::UnicodeString* quoteBuf) {
    const auto& src = Require(text);
    for (std::int32_t i = 0; i < src.Length(); ++i) {
        _ZN6icu_6811ICU_Utility12appendToRuleERNS_13UnicodeStringEiaaS2__nid_postfix(rule, _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(&src, i), isLiteral, escapeUnprintable, quoteBuf);
    }
}

void APS5_VABI _ZN6icu_6811ICU_Utility12appendToRuleERNS_13UnicodeStringEPKNS_14UnicodeMatcherEaS2__nid_postfix(Icu::Abi::UnicodeString* rule, const void* matcher, std::int8_t escapeUnprintable, Icu::Abi::UnicodeString* quoteBuf) {
    Require(rule);
    Require(quoteBuf);
    if (matcher != nullptr) {
        Icu::Abi::UnicodeString pat;
        _ZN6icu_6813UnicodeStringC1EPKDs_nid_postfix(&pat, u"");
        using ToPatternFn = Icu::Abi::UnicodeString* (APS5_VABI*)(const void*, Icu::Abi::UnicodeString*, std::int8_t);
        auto fn = Virtual_nid_no_patch<ToPatternFn>(matcher, 3);
        fn(matcher, &pat, escapeUnprintable);
        _ZN6icu_6811ICU_Utility12appendToRuleERNS_13UnicodeStringERKS1_aaS2__nid_postfix(rule, &pat, 1, escapeUnprintable, quoteBuf);
        _ZN6icu_6813UnicodeStringD1Ev_nid_postfix(&pat);
    }
}
}
