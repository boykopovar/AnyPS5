// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1999-2015, International Business Machines Corporation and others. All Rights Reserved.
// Copyright (C) 1999-2014, International Business Machines Corporation and others. All Rights Reserved.
#include "prx/libSceIcu/libSceIcu.hpp"
#include <limits>

namespace {
using String = Icu::Abi::UnicodeString;
using Set = Icu::Abi::UnicodeSet;
template<typename T> T& Require(T* pointer) {
    if (!pointer) throw std::invalid_argument("UnicodeSet pattern: null pointer");
    return *pointer;
}
void Append(String& text, std::int32_t point) { _ZN6icu_6813UnicodeString6appendEi_nid_postfix(&text, point); }
void Truncate(String& text, std::int32_t length) {
    if ((text.Flags() & String::IsBogus) && length == 0) text.Empty();
    else if (static_cast<std::uint32_t>(length) < static_cast<std::uint32_t>(text.Length())) text.SetLength(length);
}
void Escape(String& text, std::int32_t point) {
    Append(text, '\\');
    const auto bits = static_cast<std::uint32_t>(point);
    const auto digits = bits > 0xffff ? 8 : 4;
    Append(text, digits == 8 ? 'U' : 'u');
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) Append(text, u"0123456789ABCDEF"[(bits >> shift) & 15]);
}
void AppendPoint(String& text, std::int32_t point, std::int8_t escape) {
    if (escape && _ZN6icu_6811ICU_Utility13isUnprintableEi_nid_postfix(point)) { Escape(text, point); return; }
    if (point == '[' || point == ']' || point == '-' || point == '^' || point == '&' || point == '\\' || point == '{' || point == '}' || point == ':' || point == '$' || _ZN6icu_6812PatternProps12isWhiteSpaceEi_nid_postfix(point)) Append(text, '\\');
    Append(text, point);
}
void AppendString(String& result, const String& text, std::int8_t escape) {
    for (std::int32_t i = 0; i < text.Length();) {
        const auto point = _ZNK6icu_6813UnicodeString8char32AtEi_nid_postfix(&text, i);
        AppendPoint(result, point, escape);
        i += point > 0xffff ? 2 : 1;
    }
}
String* Generate(const Set& set, String& result, std::int8_t escape) {
    Append(result, '[');
    const auto ranges = set.len / 2;
    const bool inverse = ranges > 1 && Require(set.list) == 0 && set.list[ranges * 2 - 1] == 0x110000;
    if (inverse) Append(result, '^');
    for (std::int32_t i = inverse ? 1 : 0; i < ranges; ++i) {
        const auto start = inverse ? set.list[i * 2 - 1] : set.list[i * 2];
        const auto end = inverse ? set.list[i * 2] - 1 : set.list[i * 2 + 1] - 1;
        AppendPoint(result, start, escape);
        if (start != end) {
            if (start + 1 != end) Append(result, '-');
            AppendPoint(result, end, escape);
        }
    }
    if (set.strings) for (std::int32_t i = 0; i < set.strings->count; ++i) {
        Append(result, '{');
        AppendString(result, Require(static_cast<const String*>(_ZNK6icu_687UVector9elementAtEi_nid_postfix(set.strings, i))), escape);
        Append(result, '}');
    }
    Append(result, ']');
    return &result;
}
}

extern "C" {
void APS5_VABI _ZN6icu_6810UnicodeSet12_appendToPatERNS_13UnicodeStringEia_nid_postfix(String* result, std::int32_t point, std::int8_t escape) {
    AppendPoint(Require(result), point, escape);
}
void APS5_VABI _ZN6icu_6810UnicodeSet12_appendToPatERNS_13UnicodeStringERKS1_a_nid_postfix(String* result, const String* text, std::int8_t escape) {
    AppendString(Require(result), Require(text), escape);
}
String* APS5_VABI _ZNK6icu_6810UnicodeSet16_generatePatternERNS_13UnicodeStringEa_nid_postfix(const Set* self, String* result, std::int8_t escape) {
    return Generate(Require(self), Require(result), escape);
}
String* APS5_VABI _ZNK6icu_6810UnicodeSet10_toPatternERNS_13UnicodeStringEa_nid_postfix(const Set* self, String* result, std::int8_t escape) {
    const auto& set = Require(self);
    auto& output = Require(result);
    if (!set.pat) return Generate(set, output, escape);
    std::int32_t slashes = 0;
    for (std::int32_t i = 0; i < set.patLen;) {
        std::int32_t point = set.pat[i++];
        if (point >= 0xd800 && point <= 0xdbff && i < set.patLen && set.pat[i] >= 0xdc00 && set.pat[i] <= 0xdfff) point = 0x10000 + ((point - 0xd800) << 10) + (set.pat[i++] - 0xdc00);
        if (escape && _ZN6icu_6811ICU_Utility13isUnprintableEi_nid_postfix(point)) {
            if (slashes & 1) Truncate(output, output.Length() - 1);
            Escape(output, point);
            slashes = 0;
        } else {
            Append(output, point);
            if (point == '\\') ++slashes;
            else slashes = 0;
        }
    }
    return result;
}
String* APS5_VABI _ZNK6icu_6810UnicodeSet9toPatternERNS_13UnicodeStringEa_nid_postfix(const Set* self, String* result, std::int8_t escape) {
    Truncate(Require(result), 0);
    return _ZNK6icu_6810UnicodeSet10_toPatternERNS_13UnicodeStringEa_nid_postfix(self, result, escape);
}
String* APS5_VABI _ZThn8_NK6icu_6810UnicodeSet9toPatternERNS_13UnicodeStringEa_nid_postfix(const void* self, String* result, std::int8_t escape) {
    return _ZNK6icu_6810UnicodeSet9toPatternERNS_13UnicodeStringEa_nid_postfix(reinterpret_cast<const Set*>(static_cast<const char*>(&Require(static_cast<const char*>(self))) - 8), result, escape);
}
std::int8_t APS5_VABI _ZN6icu_6810UnicodeSet24resemblesPropertyPatternERKNS_13UnicodeStringEi_nid_postfix(const String* pattern, std::int32_t position) {
    if (position > std::numeric_limits<std::int32_t>::max() - 5) throw std::invalid_argument("UnicodeSet pattern: position overflow");
    if (position + 5 > Require(pattern).Length()) return 0;
    const auto first = _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(pattern, position);
    const auto second = _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(pattern, position + 1);
    return (first == '[' && second == ':') || (first == '\\' && (second == 'p' || second == 'P' || second == 'N'));
}
std::int8_t APS5_VABI _ZN6icu_6810UnicodeSet16resemblesPatternERKNS_13UnicodeStringEi_nid_postfix(const String* pattern, std::int32_t position) {
    if (position == std::numeric_limits<std::int32_t>::max()) throw std::invalid_argument("UnicodeSet pattern: position overflow");
    return (position + 1 < Require(pattern).Length() && _ZNK6icu_6813UnicodeString9getCharAtEi_nid_postfix(pattern, position) == '[') || _ZN6icu_6810UnicodeSet24resemblesPropertyPatternERKNS_13UnicodeStringEi_nid_postfix(pattern, position);
}
}
