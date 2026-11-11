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
 * Derived from Source/WTF/wtf/text/StringImpl.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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
 * Derived from Source/WTF/wtf/text/WTFString.cpp@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2005-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Derived from Source/WTF/wtf/text/StringHasher.h@c55364d53f971f63a709976f168d932d558b8909
 */
/*
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 * Derived from Source/WTF/wtf/text/StringToIntegerConversion.h@c55364d53f971f63a709976f168d932d558b8909
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/include/WtfStringCreateExports.hpp"
#include "prx/libSceJsc/include/WtfStringSearchExports.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Jsc::WtfAbi::ASCIILiteral;
using Jsc::WtfAbi::CodeUnitMatchFunction;
using Jsc::WtfAbi::String;
using Jsc::WtfAbi::StringImpl;
using Jsc::WtfAbi::StringVector;

constexpr unsigned MaxLength = 0x7FFFFFFFu;
constexpr std::size_t NotFound = std::numeric_limits<std::size_t>::max();
constexpr std::size_t TailOffset = 20;
constexpr unsigned OwnershipInternal = 0;
constexpr unsigned OwnershipOwned = 1;
constexpr unsigned OwnershipSubstring = 2;

struct AllocationHeader {
    void* raw;
    std::size_t size;
};

constexpr std::size_t AllocationAlignment = 16;

void* Allocate(std::size_t size) {
    constexpr std::size_t slack = AllocationAlignment + sizeof(AllocationHeader);
    if (size > std::numeric_limits<std::size_t>::max() - slack) throw std::bad_alloc();
    void* raw = std::malloc(size + slack);
    if (!raw) throw std::bad_alloc();
    const auto rawAddress = reinterpret_cast<std::uintptr_t>(raw);
    const std::uintptr_t payload = (rawAddress + sizeof(AllocationHeader) + AllocationAlignment - 1) & ~std::uintptr_t{AllocationAlignment - 1};
    auto* header = reinterpret_cast<AllocationHeader*>(payload - sizeof(AllocationHeader));
    header->raw = raw;
    header->size = size;
    return reinterpret_cast<void*>(payload);
}

void Deallocate(void* pointer) {
    if (!pointer) return;
    std::free(reinterpret_cast<AllocationHeader*>(static_cast<char*>(pointer) - sizeof(AllocationHeader))->raw);
}

constexpr unsigned EmptyStringHash() {
    unsigned result = 0x9E3779B9u;
    result ^= result << 3;
    result += result >> 5;
    result ^= result << 2;
    result += result >> 15;
    result ^= result << 10;
    result &= 0x00FFFFFFu;
    return result ? result : 0x00800000u;
}

struct Text {
    const void* data;
    unsigned length;
    bool is8Bit;

    char16_t operator[](unsigned index) const {
        return is8Bit ? static_cast<const unsigned char*>(data)[index] : static_cast<const char16_t*>(data)[index];
    }
};

bool Is8Bit(const StringImpl& impl) {
    return impl.hashAndFlags & Jsc::WtfAbi::StringImplFlag8Bit;
}

Text View(const StringImpl& impl) {
    return {impl.data, impl.length, Is8Bit(impl)};
}

template<typename T> T& Require(T* value, const char* function) {
    if (!value) throw std::invalid_argument(std::string(function) + ": null WTF string argument");
    return *value;
}

template<typename T> void RequireCharacters(T characters, const char* function) {
    if (!characters) throw std::invalid_argument(std::string(function) + ": null character pointer");
}

StringImpl* Ref(StringImpl& impl) {
    impl.refCount += 2;
    return &impl;
}

StringImpl& EmptyImpl() {
    static const unsigned char characters[1] = {};
    static StringImpl impl{1, 0, characters, (EmptyStringHash() << 8) | Jsc::WtfAbi::StringImplFlagAtom | Jsc::WtfAbi::StringImplFlagDidReportCost | Jsc::WtfAbi::StringImplFlag8Bit};
    return impl;
}

String Empty() {
    return String(Ref(EmptyImpl()));
}

void Deref(StringImpl* impl);

void Destroy(StringImpl* impl) {
    if (impl->hashAndFlags & (Jsc::WtfAbi::StringImplFlagAtom | Jsc::WtfAbi::StringImplFlagSymbol))
        throw std::logic_error("WTF::StringImpl::destroy: atom and symbol strings are not modeled");
    switch (impl->hashAndFlags & Jsc::WtfAbi::StringImplMaskBufferOwnership) {
    case OwnershipInternal:
        break;
    case OwnershipOwned:
        Deallocate(const_cast<void*>(impl->data));
        break;
    case OwnershipSubstring:
        Deref(*reinterpret_cast<StringImpl**>(reinterpret_cast<unsigned char*>(impl) + sizeof(StringImpl)));
        break;
    default:
        throw std::logic_error("WTF::StringImpl::destroy: external buffers are not modeled");
    }
    Deallocate(impl);
}

void Deref(StringImpl* impl) {
    if (!impl) return;
    if (impl->refCount == 2) {
        Destroy(impl);
        return;
    }
    impl->refCount -= 2;
}

std::size_t MaxInternalLength(bool is8Bit) {
    return std::min<std::size_t>(MaxLength, (std::numeric_limits<unsigned>::max() - sizeof(StringImpl)) / (is8Bit ? 1 : 2));
}

String CreateUninitialized(std::size_t length, bool is8Bit, void*& data) {
    if (!length) {
        data = nullptr;
        return Empty();
    }
    if (length > MaxInternalLength(is8Bit)) throw std::length_error("WTF::StringImpl: length above the maximum internal length");
    void* storage = Allocate(std::max(sizeof(StringImpl), TailOffset + length * (is8Bit ? 1 : 2)));
    data = static_cast<unsigned char*>(storage) + TailOffset;
    return String(new (storage) StringImpl{2, static_cast<unsigned>(length), data, is8Bit ? Jsc::WtfAbi::StringImplFlag8Bit : 0u});
}

void Store(void* data, bool is8Bit, std::size_t index, char16_t character) {
    if (is8Bit) static_cast<unsigned char*>(data)[index] = static_cast<unsigned char>(character);
    else static_cast<char16_t*>(data)[index] = character;
}

void CopyText(void* data, bool is8Bit, std::size_t offset, Text source, unsigned start, unsigned count) {
    for (unsigned i = 0; i < count; ++i) Store(data, is8Bit, offset + i, source[start + i]);
}

String CreateCopy(Text source) {
    if (!source.data || !source.length) return Empty();
    void* data;
    String result = CreateUninitialized(source.length, source.is8Bit, data);
    CopyText(data, source.is8Bit, 0, source, 0, source.length);
    return result;
}

String CreateWithoutCopying(const void* characters, unsigned length, bool is8Bit) {
    void* storage = Allocate(sizeof(StringImpl));
    return String(new (storage) StringImpl{2, length, characters, is8Bit ? Jsc::WtfAbi::StringImplFlag8Bit : 0u});
}

template<typename T> std::size_t NullTerminatedLength(const T* string) {
    std::size_t length = 0;
    while (string[length]) ++length;
    return length;
}

bool IsSpaceOrNewline(char16_t character) {
    if (character <= 0xFF) return character == ' ' || (character >= 0x9 && character <= 0xD);
    return character == 0x1680 || (character >= 0x2000 && character <= 0x200A) || character == 0x2028 || character == 0x205F || character == 0x3000;
}

template<typename Predicate> String StripMatched(StringImpl& impl, Predicate predicate) {
    const Text text = View(impl);
    if (!text.length) return String(Ref(impl));
    unsigned start = 0;
    unsigned end = text.length - 1;
    while (start <= end && predicate(text[start])) ++start;
    if (start > end) return Empty();
    while (end && predicate(text[end])) --end;
    if (!start && end == text.length - 1) return String(Ref(impl));
    return CreateCopy({static_cast<const unsigned char*>(text.data) + std::size_t(start) * (text.is8Bit ? 1 : 2), end + 1 - start, text.is8Bit});
}

template<typename Predicate> String SimplifyMatched(StringImpl& impl, Predicate predicate) {
    const Text text = View(impl);
    std::u16string output;
    output.reserve(text.length);
    bool changedToSpace = false;
    unsigned from = 0;
    while (true) {
        while (from != text.length && predicate(text[from])) {
            if (text[from] != ' ') changedToSpace = true;
            ++from;
        }
        while (from != text.length && !predicate(text[from])) output.push_back(text[from++]);
        if (from == text.length) break;
        output.push_back(u' ');
    }
    if (!output.empty() && output.back() == u' ') output.pop_back();
    if (output.size() == text.length && !changedToSpace) return String(Ref(impl));
    if (output.empty()) return Empty();
    void* data = Allocate(output.size() * (text.is8Bit ? 1 : 2));
    for (std::size_t i = 0; i < output.size(); ++i) Store(data, text.is8Bit, i, output[i]);
    try {
        void* storage = Allocate(sizeof(StringImpl));
        return String(new (storage) StringImpl{2, static_cast<unsigned>(output.size()), data, (text.is8Bit ? Jsc::WtfAbi::StringImplFlag8Bit : 0u) | OwnershipOwned});
    } catch (...) {
        Deallocate(data);
        throw;
    }
}

String Substring(StringImpl& impl, unsigned start, unsigned length) {
    if (start >= impl.length) return Empty();
    const unsigned maxLength = impl.length - start;
    if (length >= maxLength) {
        if (!start) return String(Ref(impl));
        length = maxLength;
    }
    const Text text = View(impl);
    return CreateCopy({static_cast<const unsigned char*>(text.data) + std::size_t(start) * (text.is8Bit ? 1 : 2), length, text.is8Bit});
}

std::size_t FindCharacter(Text text, char16_t character, std::size_t start) {
    if (text.is8Bit && character > 0xFF) return NotFound;
    for (std::size_t i = start; i < text.length; ++i) {
        if (text[static_cast<unsigned>(i)] == character) return i;
    }
    return NotFound;
}

String ReplaceCharacter(StringImpl& impl, char16_t pattern, Text replacement) {
    const Text text = View(impl);
    unsigned matchCount = 0;
    for (std::size_t start = 0; (start = FindCharacter(text, pattern, start)) != NotFound; ++start) ++matchCount;
    if (!matchCount) return String(Ref(impl));
    if (replacement.length && matchCount > MaxLength / replacement.length) throw std::length_error("WTF::StringImpl::replace: result length above the maximum");
    const unsigned replaceSize = matchCount * replacement.length;
    unsigned newSize = text.length - matchCount;
    if (newSize >= MaxLength - replaceSize) throw std::length_error("WTF::StringImpl::replace: result length above the maximum");
    newSize += replaceSize;
    const bool is8Bit = text.is8Bit && replacement.is8Bit;
    void* data;
    String result = CreateUninitialized(newSize, is8Bit, data);
    std::size_t segmentStart = 0;
    std::size_t offset = 0;
    std::size_t segmentEnd;
    while ((segmentEnd = FindCharacter(text, pattern, segmentStart)) != NotFound) {
        const auto segmentLength = static_cast<unsigned>(segmentEnd - segmentStart);
        CopyText(data, is8Bit, offset, text, static_cast<unsigned>(segmentStart), segmentLength);
        offset += segmentLength;
        CopyText(data, is8Bit, offset, replacement, 0, replacement.length);
        offset += replacement.length;
        segmentStart = segmentEnd + 1;
    }
    CopyText(data, is8Bit, offset, text, static_cast<unsigned>(segmentStart), text.length - static_cast<unsigned>(segmentStart));
    return result;
}

void Assign(String& target, String&& value) {
    Deref(target.impl);
    target.impl = value.Release();
}

void ConstructFromLChars(String* self, const unsigned char* string) {
    Require(self, __func__).impl = string ? _ZN3WTF10StringImpl6createEPKh(string).Release() : nullptr;
}

void ConstructFromUChars(String* self, const char16_t* string) {
    Require(self, __func__);
    if (!string) {
        self->impl = nullptr;
        return;
    }
    const std::size_t length = NullTerminatedLength(string);
    if (length >= MaxLength) throw std::length_error("WTF::String: null-terminated length above the maximum");
    self->impl = CreateCopy({string, static_cast<unsigned>(length), false}).Release();
}

void ConstructFromLiteral(String* self, ASCIILiteral characters) {
    Require(self, __func__).impl = _ZN3WTF10StringImpl17createFromLiteralEPKc(characters.characters).Release();
}

void AppendText(String& self, Text other) {
    StringImpl& impl = *self.impl;
    const Text text = View(impl);
    if (other.length > MaxLength - text.length) throw std::length_error("WTF::String::append: result length above the maximum");
    const bool is8Bit = text.is8Bit && other.is8Bit;
    void* data;
    String result = CreateUninitialized(std::size_t(text.length) + other.length, is8Bit, data);
    CopyText(data, is8Bit, 0, text, 0, text.length);
    CopyText(data, is8Bit, text.length, other, 0, other.length);
    Assign(self, std::move(result));
}

template<bool AllowEmptyEntries> StringVector Split(const String* self, const String* separator) {
    const String& string = Require(self, __func__);
    const String& pattern = Require(separator, __func__);
    std::vector<StringImpl*> parts;
    try {
        const unsigned length = string.impl ? string.impl->length : 0;
        unsigned start = 0;
        if (string.impl && pattern.impl) {
            if (!pattern.impl->length) throw std::invalid_argument("WTF::String::split: empty separator loops forever in WebKit");
            std::size_t end;
            while ((end = _ZN3WTF10StringImpl4findEPS0_j(string.impl, pattern.impl, start)) != NotFound) {
                if (AllowEmptyEntries || start != end) parts.push_back(Substring(*string.impl, start, static_cast<unsigned>(end) - start).Release());
                start = static_cast<unsigned>(end) + pattern.impl->length;
            }
        }
        if (AllowEmptyEntries || start != length) parts.push_back(string.impl ? Substring(*string.impl, start, MaxLength).Release() : nullptr);
        StringVector result;
        if (!parts.empty()) {
            result.buffer = static_cast<String*>(Allocate(parts.size() * sizeof(String)));
            result.capacity = static_cast<unsigned>(parts.size());
            for (std::size_t i = 0; i < parts.size(); ++i) new (&result.buffer[i]) String(parts[i]);
            result.size = result.capacity;
        }
        return result;
    } catch (...) {
        for (StringImpl* part : parts) Deref(part);
        throw;
    }
}

bool IsASCIIDigit(char16_t character) {
    return character >= '0' && character <= '9';
}

bool IsASCIIAlpha(char16_t character) {
    return (character | 0x20) >= 'a' && (character | 0x20) <= 'z';
}

bool IsCharacterAllowedInBase(char16_t character, int base) {
    if (character > 0x7F) return false;
    if (IsASCIIDigit(character)) return character - '0' < base;
    if (IsASCIIAlpha(character)) {
        if (base > 36) base = 36;
        return (character >= 'a' && character < 'a' + base - 10) || (character >= 'A' && character < 'A' + base - 10);
    }
    return false;
}

unsigned ToUIntStrict(Text text, bool* ok, int base) {
    if (!base) throw std::invalid_argument("WTF::String::toUIntStrict: base 0 divides by zero in WebKit");
    constexpr unsigned integralMax = std::numeric_limits<unsigned>::max();
    const unsigned divisor = static_cast<unsigned>(base);
    const unsigned maxMultiplier = integralMax / divisor;
    unsigned value = 0;
    bool isOk = false;
    unsigned index = 0;
    unsigned length = text.length;
    while (length && IsSpaceOrNewline(text[index])) {
        --length;
        ++index;
    }
    if (length && text[index] == '+') {
        --length;
        ++index;
    }
    if (length && IsCharacterAllowedInBase(text[index], base)) {
        bool overflow = false;
        while (length && IsCharacterAllowedInBase(text[index], base)) {
            --length;
            const char16_t character = text[index];
            unsigned digit;
            if (IsASCIIDigit(character)) digit = character - '0';
            else if (character >= 'a') digit = character - 'a' + 10;
            else digit = character - 'A' + 10;
            if (value > maxMultiplier || (value == maxMultiplier && digit > integralMax % divisor)) {
                overflow = true;
                break;
            }
            value = divisor * value + digit;
            ++index;
        }
        if (!overflow) {
            while (length && IsSpaceOrNewline(text[index])) {
                --length;
                ++index;
            }
            isOk = !length;
        }
    }
    if (ok) *ok = isOk;
    return isOk ? value : 0;
}

}  // namespace

namespace Jsc::WtfAbi {

String::~String() {
    Deref(impl);
}

StringVector::~StringVector() {
    for (unsigned i = 0; i < size; ++i) buffer[i].~String();
    Deallocate(buffer);
}

}  // namespace Jsc::WtfAbi

extern "C" {

String APS5_VABI _ZN3WTF10StringImpl6createEPKh(const unsigned char* string) {
    if (!string) return Empty();
    const std::size_t length = std::strlen(reinterpret_cast<const char*>(string));
    if (length > MaxLength) throw std::length_error("WTF::StringImpl::create: length above the maximum");
    return CreateCopy({string, static_cast<unsigned>(length), true});
}

String APS5_VABI _ZN3WTF10StringImpl20create8BitIfPossibleEPKDs(const char16_t* string) {
    RequireCharacters(string, __func__);
    const std::size_t length = NullTerminatedLength(string);
    if (length >= MaxLength) throw std::length_error("WTF::StringImpl::create8BitIfPossible: length above the maximum");
    const bool latin1 = std::all_of(string, string + length, [](char16_t character) { return character <= 0xFF; });
    if (!length) return Empty();
    void* data;
    String result = CreateUninitialized(length, latin1, data);
    CopyText(data, latin1, 0, {string, static_cast<unsigned>(length), false}, 0, static_cast<unsigned>(length));
    return result;
}

String APS5_VABI _ZN3WTF10StringImpl17createFromLiteralEPKc(const char* characters) {
    RequireCharacters(characters, __func__);
    const std::size_t length = std::strlen(characters);
    if (length > std::numeric_limits<unsigned>::max()) throw std::length_error("WTF::StringImpl::createFromLiteral: length above 32 bits");
    return CreateWithoutCopying(characters, static_cast<unsigned>(length), true);
}

String APS5_VABI _ZN3WTF10StringImpl17createFromLiteralEPKcj(const char* characters, unsigned length) {
    return CreateWithoutCopying(characters, length, true);
}

String APS5_VABI _ZN3WTF10StringImpl20createWithoutCopyingEPKDsj(const char16_t* characters, unsigned length) {
    if (!length) return Empty();
    return CreateWithoutCopying(characters, length, false);
}

String APS5_VABI _ZN3WTF10StringImpl20createWithoutCopyingEPKhj(const unsigned char* characters, unsigned length) {
    if (!length) return Empty();
    return CreateWithoutCopying(characters, length, true);
}

String APS5_VABI _ZN3WTF10StringImpl7replaceEDsPS0_(StringImpl* self, char16_t pattern, StringImpl* replacement) {
    StringImpl& impl = Require(self, __func__);
    if (!replacement) return String(Ref(impl));
    return ReplaceCharacter(impl, pattern, View(*replacement));
}

String APS5_VABI _ZN3WTF10StringImpl7replaceEPS0_S1_(StringImpl* self, StringImpl* pattern, StringImpl* replacement) {
    StringImpl& impl = Require(self, __func__);
    if (!pattern || !replacement) return String(Ref(impl));
    const unsigned patternLength = pattern->length;
    if (!patternLength) return String(Ref(impl));
    const Text text = View(impl);
    const Text with = View(*replacement);
    unsigned matchCount = 0;
    for (std::size_t start = 0; (start = _ZN3WTF10StringImpl4findEPS0_j(self, pattern, static_cast<unsigned>(start))) != NotFound; start += patternLength) ++matchCount;
    if (!matchCount) return String(Ref(impl));
    unsigned newSize = text.length - matchCount * patternLength;
    if (with.length && matchCount > MaxLength / with.length) throw std::length_error("WTF::StringImpl::replace: result length above the maximum");
    if (newSize > MaxLength - matchCount * with.length) throw std::length_error("WTF::StringImpl::replace: result length above the maximum");
    newSize += matchCount * with.length;
    const bool is8Bit = text.is8Bit && with.is8Bit;
    void* data;
    String result = CreateUninitialized(newSize, is8Bit, data);
    std::size_t segmentStart = 0;
    std::size_t offset = 0;
    std::size_t segmentEnd;
    while ((segmentEnd = _ZN3WTF10StringImpl4findEPS0_j(self, pattern, static_cast<unsigned>(segmentStart))) != NotFound) {
        const auto segmentLength = static_cast<unsigned>(segmentEnd - segmentStart);
        CopyText(data, is8Bit, offset, text, static_cast<unsigned>(segmentStart), segmentLength);
        offset += segmentLength;
        CopyText(data, is8Bit, offset, with, 0, with.length);
        offset += with.length;
        segmentStart = segmentEnd + patternLength;
    }
    CopyText(data, is8Bit, offset, text, static_cast<unsigned>(segmentStart), text.length - static_cast<unsigned>(segmentStart));
    return result;
}

String APS5_VABI _ZN3WTF10StringImpl7replaceEjjPS0_(StringImpl* self, unsigned position, unsigned lengthToReplace, StringImpl* string) {
    StringImpl& impl = Require(self, __func__);
    position = std::min(position, impl.length);
    lengthToReplace = std::min(lengthToReplace, impl.length - position);
    const unsigned lengthToInsert = string ? string->length : 0;
    if (!lengthToReplace && !lengthToInsert) return String(Ref(impl));
    if (impl.length - lengthToReplace >= MaxLength - lengthToInsert) throw std::length_error("WTF::StringImpl::replace: result length above the maximum");
    const Text text = View(impl);
    const Text inserted = string ? View(*string) : Text{nullptr, 0, true};
    const bool is8Bit = text.is8Bit && inserted.is8Bit;
    void* data;
    String result = CreateUninitialized(std::size_t(impl.length) - lengthToReplace + lengthToInsert, is8Bit, data);
    CopyText(data, is8Bit, 0, text, 0, position);
    CopyText(data, is8Bit, position, inserted, 0, lengthToInsert);
    CopyText(data, is8Bit, std::size_t(position) + lengthToInsert, text, position + lengthToReplace, impl.length - position - lengthToReplace);
    return result;
}

String APS5_VABI _ZN3WTF10StringImpl18simplifyWhiteSpaceEv(StringImpl* self) {
    return SimplifyMatched(Require(self, __func__), IsSpaceOrNewline);
}

void APS5_VABI _ZN3WTF6StringC1ENS_12ASCIILiteralE(String* self, ASCIILiteral characters) {
    ConstructFromLiteral(self, characters);
}

void APS5_VABI _ZN3WTF6StringC2ENS_12ASCIILiteralE(String* self, ASCIILiteral characters) {
    ConstructFromLiteral(self, characters);
}

void APS5_VABI _ZN3WTF6StringC1EPKDs(String* self, const char16_t* string) {
    ConstructFromUChars(self, string);
}

void APS5_VABI _ZN3WTF6StringC2EPKDs(String* self, const char16_t* string) {
    ConstructFromUChars(self, string);
}

void APS5_VABI _ZN3WTF6StringC1EPKh(String* self, const unsigned char* string) {
    ConstructFromLChars(self, string);
}

void APS5_VABI _ZN3WTF6StringC2EPKh(String* self, const unsigned char* string) {
    ConstructFromLChars(self, string);
}

String APS5_VABI _ZN3WTF6String23make16BitFrom8BitSourceEPKhm(const unsigned char* source, std::size_t length) {
    if (!length) return String();
    RequireCharacters(source, __func__);
    if (length > MaxLength) throw std::length_error("WTF::String::make16BitFrom8BitSource: length above the maximum");
    void* data;
    String result = CreateUninitialized(length, false, data);
    CopyText(data, false, 0, {source, static_cast<unsigned>(length), true}, 0, static_cast<unsigned>(length));
    return result;
}

String APS5_VABI _ZN3WTF6String23make8BitFrom16BitSourceEPKDsm(const char16_t* source, std::size_t length) {
    if (!length) return String();
    RequireCharacters(source, __func__);
    if (length > MaxLength) throw std::length_error("WTF::String::make8BitFrom16BitSource: length above the maximum");
    void* data;
    String result = CreateUninitialized(length, true, data);
    CopyText(data, true, 0, {source, static_cast<unsigned>(length), false}, 0, static_cast<unsigned>(length));
    return result;
}

void APS5_VABI _ZN3WTF6String6appendEDs(String* self, char16_t character) {
    String& string = Require(self, __func__);
    if (!string.impl) {
        string.impl = CreateCopy({&character, 1, false}).Release();
        return;
    }
    if (string.impl->length >= MaxLength) throw std::length_error("WTF::String::append: result length above the maximum");
    const bool is8Bit = Is8Bit(*string.impl) && character <= 0xFF;
    const unsigned char narrow = static_cast<unsigned char>(character);
    AppendText(string, is8Bit ? Text{&narrow, 1, true} : Text{&character, 1, false});
}

void APS5_VABI _ZN3WTF6String6appendEPKDsj(String* self, const char16_t* characters, unsigned length) {
    String& string = Require(self, __func__);
    if (!string.impl) {
        if (characters) string.impl = CreateCopy({characters, length, false}).Release();
        return;
    }
    if (!length) return;
    RequireCharacters(characters, __func__);
    if (length > MaxLength - string.impl->length) throw std::length_error("WTF::String::append: result length above the maximum");
    const Text text = View(*string.impl);
    void* data;
    String result = CreateUninitialized(std::size_t(text.length) + length, false, data);
    CopyText(data, false, 0, text, 0, text.length);
    CopyText(data, false, text.length, {characters, length, false}, 0, length);
    Assign(string, std::move(result));
}

void APS5_VABI _ZN3WTF6String6appendEPKhj(String* self, const unsigned char* characters, unsigned length) {
    String& string = Require(self, __func__);
    if (!string.impl) {
        if (characters) string.impl = CreateCopy({characters, length, true}).Release();
        return;
    }
    if (!length) return;
    RequireCharacters(characters, __func__);
    AppendText(string, {characters, length, true});
}

void APS5_VABI _ZN3WTF6String6appendERKS0_(String* self, const String* string) {
    String& target = Require(self, __func__);
    const String& other = Require(string, __func__);
    if (!target.impl) {
        target.impl = other.impl ? Ref(*other.impl) : nullptr;
        return;
    }
    if (!other.impl || !other.impl->length) return;
    AppendText(target, View(*other.impl));
}

void APS5_VABI _ZN3WTF6String6appendEh(String* self, unsigned char character) {
    String& string = Require(self, __func__);
    if (!string.impl) {
        string.impl = CreateCopy({&character, 1, true}).Release();
        return;
    }
    if (string.impl->length >= MaxLength) throw std::length_error("WTF::String::append: result length above the maximum");
    AppendText(string, {&character, 1, true});
}

void APS5_VABI _ZN3WTF6String6insertERKS0_j(String* self, const String* string, unsigned position) {
    String& target = Require(self, __func__);
    const String& other = Require(string, __func__);
    const unsigned lengthToInsert = other.impl ? other.impl->length : 0;
    if (!lengthToInsert) {
        if (other.impl && !target.impl) target.impl = Ref(*other.impl);
        return;
    }
    const unsigned length = target.impl ? target.impl->length : 0;
    if (position >= length) {
        _ZN3WTF6String6appendERKS0_(self, string);
        return;
    }
    if (lengthToInsert > MaxLength - length) throw std::length_error("WTF::String::insert: result length above the maximum");
    const Text text = View(*target.impl);
    const Text inserted = View(*other.impl);
    const bool is8Bit = text.is8Bit && inserted.is8Bit;
    void* data;
    String result = CreateUninitialized(std::size_t(length) + lengthToInsert, is8Bit, data);
    CopyText(data, is8Bit, 0, text, 0, position);
    CopyText(data, is8Bit, position, inserted, 0, lengthToInsert);
    CopyText(data, is8Bit, std::size_t(position) + lengthToInsert, text, position, length - position);
    Assign(target, std::move(result));
}

void APS5_VABI _ZN3WTF6String6removeEjj(String* self, unsigned position, unsigned lengthToRemove) {
    String& string = Require(self, __func__);
    if (!lengthToRemove) return;
    const unsigned length = string.impl ? string.impl->length : 0;
    if (position >= length) return;
    lengthToRemove = std::min(lengthToRemove, length - position);
    const Text text = View(*string.impl);
    void* data;
    String result = CreateUninitialized(length - lengthToRemove, text.is8Bit, data);
    CopyText(data, text.is8Bit, 0, text, 0, position);
    CopyText(data, text.is8Bit, position, text, position + lengthToRemove, length - lengthToRemove - position);
    Assign(string, std::move(result));
}

void APS5_VABI _ZN3WTF6String8truncateEj(String* self, unsigned length) {
    String& string = Require(self, __func__);
    if (string.impl) Assign(string, Substring(*string.impl, 0, length));
}

String APS5_VABI _ZNK3WTF6String9substringEjj(const String* self, unsigned position, unsigned length) {
    const String& string = Require(self, __func__);
    return string.impl ? Substring(*string.impl, position, length) : String();
}

String APS5_VABI _ZNK3WTF6String15stripWhiteSpaceEv(const String* self) {
    const String& string = Require(self, __func__);
    return string.impl ? StripMatched(*string.impl, IsSpaceOrNewline) : String();
}

String APS5_VABI _ZNK3WTF6String18simplifyWhiteSpaceEv(const String* self) {
    const String& string = Require(self, __func__);
    return string.impl ? SimplifyMatched(*string.impl, IsSpaceOrNewline) : String();
}

String APS5_VABI _ZNK3WTF6String33stripLeadingAndTrailingCharactersEPFbDsE(const String* self, CodeUnitMatchFunction predicate) {
    const String& string = Require(self, __func__);
    if (!string.impl) return String();
    if (string.impl->length) RequireCharacters(predicate, __func__);
    return StripMatched(*string.impl, predicate);
}

StringVector APS5_VABI _ZNK3WTF6String5splitERKS0_(const String* self, const String* separator) {
    return Split<false>(self, separator);
}

StringVector APS5_VABI _ZNK3WTF6String25splitAllowingEmptyEntriesERKS0_(const String* self, const String* separator) {
    return Split<true>(self, separator);
}

unsigned APS5_VABI _ZNK3WTF6String12toUIntStrictEPbi(const String* self, bool* ok, int base) {
    const String& string = Require(self, __func__);
    if (!string.impl) {
        if (ok) *ok = false;
        return 0;
    }
    return ToUIntStrict(View(*string.impl), ok, base);
}

const String& APS5_VABI _ZN3WTF10nullStringEv() {
    static String* const value = new String();
    return *value;
}

const String& APS5_VABI _ZN3WTF11emptyStringEv() {
    static String* const value = new String(Ref(EmptyImpl()));
    return *value;
}

}
