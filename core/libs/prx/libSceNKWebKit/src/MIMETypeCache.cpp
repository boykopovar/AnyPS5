// Derived from https://github.com/WebKit/WebKit/blob/9d2532291ea8c8182d78314fc3da10baa9d4a639/Source/WebCore/platform/graphics/MIMETypeCache.cpp
/*
 * Copyright (C) 2019-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

// Derived from https://github.com/WebKit/WebKit/blob/9d2532291ea8c8182d78314fc3da10baa9d4a639/Source/WTF/wtf/text/StringHasher.h
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
 */

// Derived from https://github.com/WebKit/WebKit/blob/9d2532291ea8c8182d78314fc3da10baa9d4a639/Source/WTF/wtf/HashTable.h
/*
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2008 David Levin <levin@chromium.org>
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
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/ContentTypeExports.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"
#include "prx/libSceNKWebKit/include/MIMETypeCacheExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::ContentType;
using NKWebKit::WebCoreAbi::kMIMETypeIsNotSupported;
using NKWebKit::WebCoreAbi::kMIMETypeIsSupported;
using NKWebKit::WebCoreAbi::kMIMETypeMayBeSupported;
using NKWebKit::WebCoreAbi::MIMETypeCache;
using NKWebKit::WebCoreAbi::MIMETypeCacheVtable;
using NKWebKit::WebCoreAbi::MIMETypeCacheVtableStorage;
using NKWebKit::WebCoreAbi::MIMETypeResult;
using NKWebKit::WebCoreAbi::MIMETypeResultMap;
using NKWebKit::WebCoreAbi::MIMETypeSet;
using NKWebKit::WtfAbi::CodeUnitAt;
using NKWebKit::WtfAbi::Release;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringVector;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::ViewOf;

constexpr std::size_t kMetadataSize = 4 * sizeof(unsigned);
constexpr int kTableSize = -1;
constexpr int kTableSizeMask = -2;
constexpr int kKeyCount = -3;
constexpr int kDeletedCount = -4;
constexpr unsigned kMinimumTableSize = 8;
constexpr unsigned kMinLoad = 6;
constexpr unsigned kMaxSmallTableCapacity = 1024;
constexpr unsigned kHashFlagCount = 8;

char16_t ToASCIILower(char16_t character) {
    return character >= u'A' && character <= u'Z' ? static_cast<char16_t>(character | 0x20) : character;
}

unsigned Hash(const StringImpl* key) {
    const StringView view = ViewOf(key);
    unsigned hash = 0x9E3779B9U;
    unsigned index = 0;
    for (; index + 1 < view.length; index += 2) {
        hash += ToASCIILower(CodeUnitAt(view, index));
        hash = (hash << 16) ^ ((static_cast<unsigned>(ToASCIILower(CodeUnitAt(view, index + 1))) << 11) ^ hash);
        hash += hash >> 11;
    }
    if (index < view.length) {
        hash += ToASCIILower(CodeUnitAt(view, index));
        hash ^= hash << 11;
        hash += hash >> 17;
    }
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 2;
    hash += hash >> 15;
    hash ^= hash << 10;
    hash &= (1U << (32 - kHashFlagCount)) - 1;
    return hash ? hash : 0x80000000U >> kHashFlagCount;
}

bool EqualIgnoringASCIICase(const StringImpl* first, const StringImpl* second) {
    const StringView a = ViewOf(first);
    const StringView b = ViewOf(second);
    if (a.length != b.length) return false;
    for (unsigned index = 0; index < a.length; ++index)
        if (ToASCIILower(CodeUnitAt(a, index)) != ToASCIILower(CodeUnitAt(b, index))) return false;
    return true;
}

StringImpl* KeyOf(StringImpl* const& bucket) {
    return bucket;
}

StringImpl* KeyOf(const MIMETypeResult& bucket) {
    return bucket.key;
}

template<typename TBucket>
unsigned* Metadata(TBucket* table) {
    return reinterpret_cast<unsigned*>(table);
}

bool ShouldExpand(std::uint64_t keyCount, std::uint64_t tableSize) {
    if (tableSize <= kMaxSmallTableCapacity) return keyCount * 4 >= tableSize * 3;
    return keyCount * 2 >= tableSize;
}

template<typename TBucket>
TBucket* AllocateTable(unsigned tableSize) {
    auto* storage = static_cast<unsigned char*>(_ZN3WTF16fastZeroedMallocEm(kMetadataSize + std::size_t(tableSize) * sizeof(TBucket)));
    auto* table = reinterpret_cast<TBucket*>(storage + kMetadataSize);
    Metadata(table)[kTableSize] = tableSize;
    Metadata(table)[kTableSizeMask] = tableSize - 1;
    return table;
}

bool IsLive(const StringImpl* key) {
    return key && key != reinterpret_cast<const StringImpl*>(~std::uintptr_t(0));
}

template<typename TBucket>
void FreeStorage(TBucket* table) {
    _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(table) - kMetadataSize);
}

template<typename TBucket>
void FreeTable(TBucket* table) {
    if (!table) return;
    const unsigned tableSize = Metadata(table)[kTableSize];
    for (unsigned index = 0; index < tableSize; ++index)
        if (IsLive(KeyOf(table[index]))) Release(KeyOf(table[index]));
    FreeStorage(table);
}

template<typename TBucket>
TBucket* EmptySlot(TBucket* table, const StringImpl* key) {
    const unsigned sizeMask = Metadata(table)[kTableSizeMask];
    unsigned index = Hash(key) & sizeMask;
    for (unsigned probeCount = 1; KeyOf(table[index]); ++probeCount) index = (index + probeCount) & sizeMask;
    return table + index;
}

template<typename TBucket>
TBucket* Find(TBucket* table, const StringImpl* key) {
    if (!table) return nullptr;
    const unsigned sizeMask = Metadata(table)[kTableSizeMask];
    unsigned index = Hash(key) & sizeMask;
    for (unsigned probeCount = 1; KeyOf(table[index]); ++probeCount) {
        if (IsLive(KeyOf(table[index])) && EqualIgnoringASCIICase(KeyOf(table[index]), key)) return table + index;
        index = (index + probeCount) & sizeMask;
    }
    return nullptr;
}

template<typename TBucket>
void Expand(TBucket*& table) {
    TBucket* oldTable = table;
    const unsigned oldSize = oldTable ? Metadata(oldTable)[kTableSize] : 0;
    const unsigned keyCount = oldTable ? Metadata(oldTable)[kKeyCount] : 0;
    unsigned newSize = kMinimumTableSize;
    if (oldSize) {
        if (keyCount * kMinLoad < oldSize * 2) {
            newSize = oldSize;
        } else {
            if (oldSize > 0x40000000U) throw std::length_error("WebCore::MIMETypeCache: hash table cannot grow further");
            newSize = oldSize * 2;
        }
    }
    table = AllocateTable<TBucket>(newSize);
    Metadata(table)[kKeyCount] = keyCount;
    for (unsigned index = 0; index < oldSize; ++index)
        if (IsLive(KeyOf(oldTable[index]))) *EmptySlot(table, KeyOf(oldTable[index])) = oldTable[index];
    if (oldTable) FreeStorage(oldTable);
}

template<typename TBucket>
void Add(TBucket*& table, const TBucket& entry) {
    if (!table) Expand(table);
    const unsigned sizeMask = Metadata(table)[kTableSizeMask];
    unsigned index = Hash(KeyOf(entry)) & sizeMask;
    TBucket* deleted = nullptr;
    for (unsigned probeCount = 1; KeyOf(table[index]); ++probeCount) {
        if (!IsLive(KeyOf(table[index])))
            deleted = table + index;
        else if (EqualIgnoringASCIICase(KeyOf(table[index]), KeyOf(entry)))
            return;
        index = (index + probeCount) & sizeMask;
    }
    TBucket* bucket = table + index;
    if (deleted) {
        bucket = deleted;
        --Metadata(table)[kDeletedCount];
    }
    KeyOf(entry)->refCount += 2;
    *bucket = entry;
    ++Metadata(table)[kKeyCount];
    if (ShouldExpand(std::uint64_t(Metadata(table)[kKeyCount]) + Metadata(table)[kDeletedCount], Metadata(table)[kTableSize])) Expand(table);
}

unsigned BestTableSize(unsigned keyCount) {
    unsigned size = keyCount - 1;
    size |= size >> 1;
    size |= size >> 2;
    size |= size >> 4;
    size |= size >> 8;
    size |= size >> 16;
    ++size;
    if (ShouldExpand(keyCount, size)) size *= 2;
    const double maxLoad = size <= kMaxSmallTableCapacity ? 3.0 / 4.0 : 1.0 / 2.0;
    const double minLoad = 1.0 / 6.0;
    const double eagerLoad = ((maxLoad + minLoad) / 2 + maxLoad) / 2;
    if (keyCount >= size * eagerLoad) size *= 2;
    return size < kMinimumTableSize ? kMinimumTableSize : size;
}

template<typename TBucket>
TBucket* Copy(TBucket* source) {
    const unsigned keyCount = source ? Metadata(source)[kKeyCount] : 0;
    if (!keyCount) return nullptr;
    TBucket* table = AllocateTable<TBucket>(BestTableSize(keyCount));
    Metadata(table)[kKeyCount] = keyCount;
    const unsigned sourceSize = Metadata(source)[kTableSize];
    for (unsigned index = 0; index < sourceSize; ++index) {
        if (!IsLive(KeyOf(source[index]))) continue;
        KeyOf(source[index])->refCount += 2;
        *EmptySlot(table, KeyOf(source[index])) = source[index];
    }
    return table;
}

MIMETypeCache& Checked(MIMETypeCache* self) {
    if (!self) throw std::invalid_argument("null WebCore::MIMETypeCache");
    return *self;
}

const MIMETypeCache& Checked(const MIMETypeCache* self) {
    if (!self) throw std::invalid_argument("null WebCore::MIMETypeCache");
    return *self;
}

const String& Checked(const String* string) {
    if (!string) throw std::invalid_argument("null WTF::String reference for WebCore::MIMETypeCache");
    return *string;
}

bool IsEmpty(const String& string) {
    return !string.impl || !string.impl->length;
}

bool IsASCIIWhitespace(char16_t character) {
    return character == u' ' || character == u'\n' || character == u'\t' || character == u'\r' || character == u'\f';
}

std::vector<std::u16string> Codecs(const ContentType& contentType) {
    const String parameter = _ZNK7WebCore11ContentType9parameterERKN3WTF6StringE(&contentType, &_ZN7WebCore11ContentType15codecsParameterEv());
    std::vector<std::u16string> codecs;
    if (!parameter.impl) return codecs;
    const StringView view = ViewOf(parameter.impl);
    unsigned start = 0;
    for (unsigned index = 0; index <= view.length; ++index) {
        if (index < view.length && CodeUnitAt(view, index) != u',') continue;
        unsigned first = start;
        unsigned last = index;
        start = index + 1;
        if (first == last) continue;
        while (first < last && IsASCIIWhitespace(CodeUnitAt(view, first))) ++first;
        while (last > first && IsASCIIWhitespace(CodeUnitAt(view, last - 1))) --last;
        std::u16string codec;
        for (unsigned position = first; position < last; ++position) codec.push_back(CodeUnitAt(view, position));
        codecs.push_back(std::move(codec));
    }
    return codecs;
}

bool EqualLettersIgnoringASCIICase(const String& string, std::u16string_view lowercaseLetters) {
    if (!string.impl) return false;
    const StringView view = ViewOf(string.impl);
    if (view.length != lowercaseLetters.size()) return false;
    for (unsigned index = 0; index < view.length; ++index)
        if (ToASCIILower(CodeUnitAt(view, index)) != lowercaseLetters[index]) return false;
    return true;
}

bool APS5_VABI IsStaticContainerType(MIMETypeCache*, StringView) {
    return false;
}

void APS5_VABI InitializeCache(MIMETypeCache*, MIMETypeSet*) {
}

const MIMETypeCacheVtableStorage vtable = {
    0,
    nullptr,
    {
        reinterpret_cast<void (APS5_VABI*)(MIMETypeCache*)>(&_ZN7WebCore13MIMETypeCacheD1Ev),
        reinterpret_cast<void (APS5_VABI*)(MIMETypeCache*)>(&_ZN7WebCore13MIMETypeCacheD0Ev),
        reinterpret_cast<bool (APS5_VABI*)(const MIMETypeCache*)>(&_ZNK7WebCore13MIMETypeCache11isAvailableEv),
        &_ZN7WebCore13MIMETypeCache13canDecodeTypeERKN3WTF6StringE,
        &_ZN7WebCore13MIMETypeCache14supportedTypesEv,
        &IsStaticContainerType,
        reinterpret_cast<bool (APS5_VABI*)(MIMETypeCache*, const String*)>(&_ZN7WebCore13MIMETypeCache26isUnsupportedContainerTypeERKN3WTF6StringE),
        &InitializeCache,
        reinterpret_cast<bool (APS5_VABI*)(MIMETypeCache*, const ContentType*)>(&_ZN7WebCore13MIMETypeCache21canDecodeExtendedTypeERKNS_11ContentTypeE),
    },
};

void CopyMembers(MIMETypeCache& self, const MIMETypeCache& other) {
    self.supportedTypes.table = other.hasSupportedTypes ? Copy(other.supportedTypes.table) : nullptr;
    self.hasSupportedTypes = other.hasSupportedTypes;
    self.cachedResults.table = other.hasCachedResults ? Copy(other.cachedResults.table) : nullptr;
    self.hasCachedResults = other.hasCachedResults;
}

void Construct(MIMETypeCache* self, const MIMETypeCache* other) {
    MIMETypeCache& object = Checked(self);
    const MIMETypeCache& source = Checked(other);
    object.vtable = &vtable.dispatch;
    CopyMembers(object, source);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore13MIMETypeCacheC1ERKS0_(MIMETypeCache* self, const MIMETypeCache* other) {
    Construct(self, other);
}

void APS5_VABI _ZN7WebCore13MIMETypeCacheC2ERKS0_(MIMETypeCache* self, const MIMETypeCache* other) {
    Construct(self, other);
}

void APS5_VABI _ZN7WebCore13MIMETypeCacheD2Ev(MIMETypeCache* self) {
    MIMETypeCache& object = Checked(self);
    if (object.hasCachedResults) FreeTable(object.cachedResults.table);
    if (object.hasSupportedTypes) FreeTable(object.supportedTypes.table);
}

MIMETypeCache* APS5_VABI _ZN7WebCore13MIMETypeCacheaSERKS0_(MIMETypeCache* self, const MIMETypeCache* other) {
    MIMETypeCache& object = Checked(self);
    MIMETypeCache copy{};
    CopyMembers(copy, Checked(other));
    if (object.hasCachedResults) FreeTable(object.cachedResults.table);
    if (object.hasSupportedTypes) FreeTable(object.supportedTypes.table);
    object.supportedTypes = copy.supportedTypes;
    object.hasSupportedTypes = copy.hasSupportedTypes;
    object.cachedResults = copy.cachedResults;
    object.hasCachedResults = copy.hasCachedResults;
    return self;
}

MIMETypeSet* APS5_VABI _ZN7WebCore13MIMETypeCache14supportedTypesEv(MIMETypeCache* self) {
    MIMETypeCache& object = Checked(self);
    if (!object.hasSupportedTypes) {
        object.supportedTypes.table = nullptr;
        object.hasSupportedTypes = true;
        object.vtable->initializeCache(self, &object.supportedTypes);
    }
    return &object.supportedTypes;
}

void APS5_VABI _ZN7WebCore13MIMETypeCache17addSupportedTypesERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(
    MIMETypeCache* self, const StringVector* types) {
    MIMETypeCache& object = Checked(self);
    if (!types) throw std::invalid_argument("null WTF::Vector<WTF::String> for WebCore::MIMETypeCache::addSupportedTypes");
    for (unsigned index = 0; index < types->size; ++index)
        if (!types->buffer[index].impl) throw std::invalid_argument("WebCore::MIMETypeCache::addSupportedTypes: null WTF::String in the list");
    if (!object.hasSupportedTypes) {
        object.supportedTypes.table = nullptr;
        object.hasSupportedTypes = true;
    }
    for (unsigned index = 0; index < types->size; ++index) Add(object.supportedTypes.table, types->buffer[index].impl);
}

bool APS5_VABI _ZN7WebCore13MIMETypeCache21supportsContainerTypeERKN3WTF6StringE(MIMETypeCache* self, const String* containerType) {
    MIMETypeCache& object = Checked(self);
    const String& type = Checked(containerType);
    if (!object.vtable->isAvailable(self) || IsEmpty(type)) return false;
    if (object.vtable->isUnsupportedContainerType(self, containerType)) return false;
    if (object.vtable->isStaticContainerType(self, ViewOf(type.impl))) return true;
    return Find(object.vtable->supportedTypes(self)->table, type.impl) != nullptr;
}

std::uint8_t APS5_VABI _ZN7WebCore13MIMETypeCache13canDecodeTypeERKN3WTF6StringE(MIMETypeCache* self, const String* mimeType) {
    MIMETypeCache& object = Checked(self);
    const String& type = Checked(mimeType);
    if (IsEmpty(type)) return kMIMETypeIsNotSupported;
    if (object.hasCachedResults)
        if (const MIMETypeResult* cached = Find(object.cachedResults.table, type.impl)) return cached->value;
    std::uint8_t result = kMIMETypeIsNotSupported;
    if (object.vtable->isAvailable(self)) {
        const ContentType contentType{String(type)};
        const String containerType = _ZNK7WebCore11ContentType13containerTypeEv(&contentType);
        if (_ZN7WebCore13MIMETypeCache21supportsContainerTypeERKN3WTF6StringE(self, &containerType)) {
            const std::vector<std::u16string> codecs = Codecs(contentType);
            if (codecs.empty())
                result = kMIMETypeMayBeSupported;
            else if (EqualLettersIgnoringASCIICase(containerType, u"audio/mpeg") && codecs.size() == 1 && codecs[0] == u"mp3")
                result = kMIMETypeIsSupported;
            else if (object.vtable->canDecodeExtendedType(self, &contentType))
                result = kMIMETypeIsSupported;
        }
    }
    if (!object.hasCachedResults) {
        object.cachedResults.table = nullptr;
        object.hasCachedResults = true;
    }
    Add(object.cachedResults.table, MIMETypeResult{type.impl, result});
    return result;
}

}
