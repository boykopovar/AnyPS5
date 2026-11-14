/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller ( mueller@kde.org )
 * Copyright (C) 2003-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Derived from Source/WTF/wtf/text/StringImpl.cpp, Source/WTF/wtf/text/WTFString.cpp and Source/WTF/wtf/text/StringView.cpp@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceIcu/CaseMapping.hpp"
#include "prx/libSceIcu/CodePoints.hpp"
#include "prx/libSceIcu/Normalization.hpp"
#include "prx/libSceIcu/Properties.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include "WtfStringAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::StringViewWithUnderlyingString;

constexpr unsigned MaxLength = std::numeric_limits<std::int32_t>::max();
constexpr unsigned char SmallLetterSharpS = 0xDF;
constexpr std::uint32_t FoldCaseDefault = 0;

using LocaleMapper = std::int32_t (APS5_VABI*)(char16_t*, std::int32_t, const char16_t*, std::int32_t, const char*, std::int32_t*);

const StringImpl& Self(const StringImpl* impl) {
    if (!impl) throw std::invalid_argument("null WTF::StringImpl");
    if (!impl->data && impl->length) throw std::invalid_argument("null WTF::StringImpl characters");
    return *impl;
}

bool Is8Bit(const StringImpl& impl) {
    return impl.hashAndFlags & 4;
}

const unsigned char* Data8(const StringImpl& impl) {
    return static_cast<const unsigned char*>(impl.data);
}

const char16_t* Data16(const StringImpl& impl) {
    return static_cast<const char16_t*>(impl.data);
}

String Retained(const StringImpl& impl) {
    auto* value = const_cast<StringImpl*>(&impl);
    value->refCount += 2;
    return String(value);
}

template<typename T> String Create(unsigned length, T*& characters) {
    void* data;
    String result(NKWebKit::WtfAbi::CreateUninitialized(length, sizeof(T) == 1, data));
    characters = static_cast<T*>(data);
    return result;
}

template<typename T> String Create(std::int32_t length, T*& characters) {
    if (length < 0) throw std::length_error("negative WTF string length");
    return Create(static_cast<unsigned>(length), characters);
}

bool IsASCIIUpper(char16_t value) {
    return value >= 'A' && value <= 'Z';
}

bool IsASCIILower(char16_t value) {
    return value >= 'a' && value <= 'z';
}

char16_t ToASCIILower(char16_t value) {
    return value | (IsASCIIUpper(value) << 5);
}

char16_t ToASCIIUpper(char16_t value) {
    return value & ~(IsASCIILower(value) << 5);
}

std::int32_t CheckedLength(const StringImpl& impl) {
    if (impl.length > MaxLength) throw std::length_error("WTF string exceeds maximum length");
    return static_cast<std::int32_t>(impl.length);
}

std::u16string Upconverted(const StringImpl& impl) {
    if (!Is8Bit(impl)) return std::u16string(Data16(impl), impl.length);
    std::u16string result(impl.length, u'\0');
    for (unsigned i = 0; i < impl.length; ++i) result[i] = Data8(impl)[i];
    return result;
}

String MapWithLocale(const StringImpl& impl, const char16_t* source, std::int32_t length, LocaleMapper mapper, const char* locale) {
    char16_t* data;
    auto result = Create(length, data);
    std::int32_t status = 0;
    std::int32_t realLength = mapper(data, length, source, length, locale, &status);
    if (status <= 0 && realLength == length) return result;
    result = Create(realLength, data);
    status = 0;
    mapper(data, realLength, source, length, locale, &status);
    if (status > 0) return Retained(impl);
    return result;
}

String LowercaseStartingAtFailingIndex8Bit(const StringImpl& impl, unsigned failingIndex) {
    if (!Is8Bit(impl) || failingIndex > impl.length) throw std::invalid_argument("invalid WTF 8-bit lowercase failing index");
    unsigned char* data;
    auto result = Create(impl.length, data);
    const auto* source = Data8(impl);
    for (unsigned i = 0; i < failingIndex; ++i) data[i] = source[i];
    for (unsigned i = failingIndex; i < impl.length; ++i) {
        auto character = source[i];
        if (character < 0x80) data[i] = static_cast<unsigned char>(ToASCIILower(character));
        else data[i] = static_cast<unsigned char>(u_tolower_nid_postfix(character));
    }
    return result;
}

String LowercaseWithoutLocale(const StringImpl& impl) {
    if (Is8Bit(impl)) {
        for (unsigned i = 0; i < impl.length; ++i) {
            auto character = Data8(impl)[i];
            if (character >= 0x80 || IsASCIIUpper(character)) return LowercaseStartingAtFailingIndex8Bit(impl, i);
        }
        return Retained(impl);
    }
    const auto* source = Data16(impl);
    bool noUpper = true;
    unsigned ored = 0;
    for (unsigned i = 0; i < impl.length; ++i) {
        if (IsASCIIUpper(source[i])) noUpper = false;
        ored |= source[i];
    }
    if (noUpper && !(ored & ~0x7FU)) return Retained(impl);
    if (!(ored & ~0x7FU)) {
        char16_t* data;
        auto result = Create(impl.length, data);
        for (unsigned i = 0; i < impl.length; ++i) data[i] = ToASCIILower(source[i]);
        return result;
    }
    return MapWithLocale(impl, source, CheckedLength(impl), u_strToLower_nid_postfix, "");
}

String UppercaseWithoutLocale(const StringImpl& impl) {
    std::int32_t length = CheckedLength(impl);
    if (Is8Bit(impl)) {
        const auto* source = Data8(impl);
        unsigned char* data;
        auto result = Create(length, data);
        unsigned ored = 0;
        for (std::int32_t i = 0; i < length; ++i) {
            ored |= source[i];
            data[i] = static_cast<unsigned char>(ToASCIIUpper(source[i]));
        }
        if (!(ored & ~0x7FU)) return result;
        unsigned sharpSCount = 0;
        bool needs16Bit = false;
        for (std::int32_t i = 0; i < length; ++i) {
            if (source[i] == SmallLetterSharpS) ++sharpSCount;
            auto upper = static_cast<char16_t>(u_toupper_nid_postfix(source[i]));
            if (upper > 0xFF) {
                needs16Bit = true;
                break;
            }
            data[i] = static_cast<unsigned char>(upper);
        }
        if (!needs16Bit) {
            if (!sharpSCount) return result;
            result = Create(impl.length + sharpSCount, data);
            auto* output = data;
            for (std::int32_t i = 0; i < length; ++i) {
                if (source[i] == SmallLetterSharpS) {
                    *output++ = 'S';
                    *output++ = 'S';
                } else {
                    *output++ = static_cast<unsigned char>(u_toupper_nid_postfix(source[i]));
                }
            }
            return result;
        }
    }
    auto source = Upconverted(impl);
    char16_t* data;
    auto result = Create(length, data);
    unsigned ored = 0;
    for (std::int32_t i = 0; i < length; ++i) {
        ored |= source[i];
        data[i] = ToASCIIUpper(source[i]);
    }
    if (!(ored & ~0x7FU)) return result;
    return MapWithLocale(impl, source.data(), length, u_strToUpper_nid_postfix, "");
}

char16_t LocaleCharacter(const String& locale, unsigned index) {
    if (!locale.impl || index >= locale.impl->length) return 0;
    const auto& impl = Self(locale.impl);
    return Is8Bit(impl) ? Data8(impl)[index] : Data16(impl)[index];
}

bool NeedsTurkishCasingRules(const String* localeIdentifier) {
    if (!localeIdentifier) throw std::invalid_argument("null WTF::AtomString");
    char16_t first = LocaleCharacter(*localeIdentifier, 0) | 0x20;
    char16_t second = LocaleCharacter(*localeIdentifier, 1) | 0x20;
    unsigned length = localeIdentifier->impl ? localeIdentifier->impl->length : 0;
    return ((first == 't' && second == 'r') || (first == 'a' && second == 'z'))
        && (length == 2 || LocaleCharacter(*localeIdentifier, 2) == '-');
}

bool Contains(const StringImpl& impl, char16_t value) {
    for (unsigned i = 0; i < impl.length; ++i) {
        if ((Is8Bit(impl) ? Data8(impl)[i] : Data16(impl)[i]) == value) return true;
    }
    return false;
}

String TurkishCase(const StringImpl& impl, LocaleMapper mapper) {
    std::int32_t length = CheckedLength(impl);
    auto source = Upconverted(impl);
    return MapWithLocale(impl, source.data(), length, mapper, "tr");
}

String LowercaseWithLocale(const StringImpl& impl, const String* localeIdentifier) {
    if (!NeedsTurkishCasingRules(localeIdentifier)) return LowercaseWithoutLocale(impl);
    return TurkishCase(impl, u_strToLower_nid_postfix);
}

String UppercaseWithLocale(const StringImpl& impl, const String* localeIdentifier) {
    if (!NeedsTurkishCasingRules(localeIdentifier) || !Contains(impl, 'i')) return UppercaseWithoutLocale(impl);
    return TurkishCase(impl, u_strToUpper_nid_postfix);
}

String FoldCase(const StringImpl& impl) {
    if (Is8Bit(impl)) {
        const auto* source = Data8(impl);
        unsigned failingIndex = 0;
        while (failingIndex < impl.length && source[failingIndex] < 0x80 && !IsASCIIUpper(source[failingIndex])) ++failingIndex;
        if (failingIndex == impl.length) return Retained(impl);
        bool needs16Bit = false;
        for (unsigned i = failingIndex; i < impl.length; ++i) {
            if (source[i] == 0xB5 || source[i] == SmallLetterSharpS) {
                needs16Bit = true;
                break;
            }
        }
        if (!needs16Bit) {
            unsigned char* data;
            auto result = Create(impl.length, data);
            for (unsigned i = 0; i < failingIndex; ++i) data[i] = source[i];
            for (unsigned i = failingIndex; i < impl.length; ++i) {
                if (source[i] < 0x80) data[i] = static_cast<unsigned char>(ToASCIILower(source[i]));
                else data[i] = static_cast<unsigned char>(u_foldCase_nid_postfix(source[i], FoldCaseDefault));
            }
            return result;
        }
    } else {
        const auto* source = Data16(impl);
        bool noUpper = true;
        unsigned ored = 0;
        for (unsigned i = 0; i < impl.length; ++i) {
            if (IsASCIIUpper(source[i])) noUpper = false;
            ored |= source[i];
        }
        if (!(ored & ~0x7FU)) {
            if (noUpper) return Retained(impl);
            char16_t* data;
            auto result = Create(impl.length, data);
            for (unsigned i = 0; i < impl.length; ++i) data[i] = ToASCIILower(source[i]);
            return result;
        }
    }
    std::int32_t length = CheckedLength(impl);
    auto source = Upconverted(impl);
    char16_t* data;
    auto result = Create(length, data);
    std::int32_t status = 0;
    std::int32_t realLength = u_strFoldCase_nid_postfix(data, length, source.data(), length, FoldCaseDefault, &status);
    if (status <= 0 && realLength == length) return result;
    result = Create(realLength, data);
    status = 0;
    u_strFoldCase_nid_postfix(data, realLength, source.data(), length, FoldCaseDefault, &status);
    if (status > 0) return Retained(impl);
    return result;
}

const StringImpl* Impl(const String* string) {
    if (!string) throw std::invalid_argument("null WTF::String");
    return string->impl;
}

StringViewWithUnderlyingString NormalizedNFC(StringView view) {
    if (view.is8Bit) return {view, String()};
    if (!view.data && view.length) throw std::invalid_argument("null WTF::StringView characters");
    if (view.length > MaxLength) throw std::length_error("WTF string exceeds maximum length");
    auto length = static_cast<std::int32_t>(view.length);
    const auto* source = static_cast<const char16_t*>(view.data);
    std::int32_t status = 0;
    const auto* normalizer = unorm2_getNFCInstance_nid_postfix(&status);
    if (status > 0 || !normalizer) throw std::runtime_error("ICU NFC normalizer unavailable");
    bool normalized = unorm2_isNormalized_nid_postfix(normalizer, source, length, &status);
    if (status > 0) throw std::runtime_error("ICU NFC check failed");
    if (normalized) return {view, String()};
    std::int32_t normalizedLength = unorm2_normalize_nid_postfix(normalizer, source, length, nullptr, 0, &status);
    char16_t* data;
    auto result = Create(normalizedLength, data);
    status = 0;
    unorm2_normalize_nid_postfix(normalizer, source, length, data, normalizedLength, &status);
    if (status > 0) throw std::runtime_error("ICU NFC normalization failed");
    StringView resultView{result.impl->data, result.impl->length, false};
    return {resultView, std::move(result)};
}

}  // namespace

extern "C" {

String APS5_VABI _ZN3WTF10StringImpl31convertToLowercaseWithoutLocaleEv(const StringImpl* self) {
    return LowercaseWithoutLocale(Self(self));
}

String APS5_VABI _ZN3WTF10StringImpl57convertToLowercaseWithoutLocaleStartingAtFailingIndex8BitEj(const StringImpl* self, unsigned failingIndex) {
    return LowercaseStartingAtFailingIndex8Bit(Self(self), failingIndex);
}

String APS5_VABI _ZN3WTF10StringImpl31convertToUppercaseWithoutLocaleEv(const StringImpl* self) {
    return UppercaseWithoutLocale(Self(self));
}

String APS5_VABI _ZN3WTF10StringImpl28convertToLowercaseWithLocaleERKNS_10AtomStringE(const StringImpl* self, const String* localeIdentifier) {
    return LowercaseWithLocale(Self(self), localeIdentifier);
}

String APS5_VABI _ZN3WTF10StringImpl28convertToUppercaseWithLocaleERKNS_10AtomStringE(const StringImpl* self, const String* localeIdentifier) {
    return UppercaseWithLocale(Self(self), localeIdentifier);
}

String APS5_VABI _ZNK3WTF6String31convertToLowercaseWithoutLocaleEv(const String* self) {
    const auto* impl = Impl(self);
    return impl ? LowercaseWithoutLocale(Self(impl)) : String();
}

String APS5_VABI _ZNK3WTF6String57convertToLowercaseWithoutLocaleStartingAtFailingIndex8BitEj(const String* self, unsigned failingIndex) {
    const auto* impl = Impl(self);
    return impl ? LowercaseStartingAtFailingIndex8Bit(Self(impl), failingIndex) : String();
}

String APS5_VABI _ZNK3WTF6String31convertToUppercaseWithoutLocaleEv(const String* self) {
    const auto* impl = Impl(self);
    return impl ? UppercaseWithoutLocale(Self(impl)) : String();
}

String APS5_VABI _ZNK3WTF6String28convertToLowercaseWithLocaleERKNS_10AtomStringE(const String* self, const String* localeIdentifier) {
    const auto* impl = Impl(self);
    return impl ? LowercaseWithLocale(Self(impl), localeIdentifier) : String();
}

String APS5_VABI _ZNK3WTF6String28convertToUppercaseWithLocaleERKNS_10AtomStringE(const String* self, const String* localeIdentifier) {
    const auto* impl = Impl(self);
    return impl ? UppercaseWithLocale(Self(impl), localeIdentifier) : String();
}

String APS5_VABI _ZNK3WTF6String8foldCaseEv(const String* self) {
    const auto* impl = Impl(self);
    return impl ? FoldCase(Self(impl)) : String();
}

StringViewWithUnderlyingString APS5_VABI _ZN3WTF13normalizedNFCENS_10StringViewE(StringView string) {
    return NormalizedNFC(string);
}

String APS5_VABI _ZN3WTF13normalizedNFCERKNS_6StringE(const String* string) {
    const auto* impl = Impl(string);
    if (impl) Self(impl);
    StringView view = impl ? StringView{impl->data, impl->length, Is8Bit(*impl)} : StringView{nullptr, 0, true};
    auto result = NormalizedNFC(view);
    if (!result.underlyingString.impl) return *string;
    return std::move(result.underlyingString);
}

}
