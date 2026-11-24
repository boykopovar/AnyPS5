// Derived from Sony PS5 WebKit 11.x revision 80f77e94c63ee7202eec3d4b5b1a021dd89011ba, Source/WebCore/loader/ResourceLoadStatistics.cpp
/*
 * Copyright (C) 2016-2022 Apple Inc.  All rights reserved.
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

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c63ee7202eec3d4b5b1a021dd89011ba, Source/WTF/wtf/HashTable.h
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

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c63ee7202eec3d4b5b1a021dd89011ba, Source/WTF/wtf/text/StringHasher.h
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

#include "prx/libSceNKWebKit/include/ResourceLoadStatisticsExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "WtfTextAbi.hpp"

#include <algorithm>
#include <cstring>
#include <new>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::KeyedDecoder;
using NKWebKit::WebCoreAbi::KeyedDecoderVtable;
using NKWebKit::WebCoreAbi::KeyedEncoder;
using NKWebKit::WebCoreAbi::KeyedEncoderVtable;
using NKWebKit::WebCoreAbi::RegistrableDomain;
using NKWebKit::WebCoreAbi::ResourceLoadStatistics;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringBuilder;
using NKWebKit::WtfAbi::StringView;

constexpr std::uintptr_t DeletedImplValue = ~static_cast<std::uintptr_t>(0);
constexpr unsigned String8BitFlag = 4;
constexpr std::size_t MetadataSize = 16;
constexpr unsigned MinimumTableSize = 8;
constexpr unsigned SmallMaxLoadNumerator = 3;
constexpr unsigned SmallMaxLoadDenominator = 4;
constexpr unsigned LargeMaxLoadNumerator = 1;
constexpr unsigned LargeMaxLoadDenominator = 2;
constexpr unsigned MaxSmallTableCapacity = 1024;
constexpr unsigned MinLoad = 6;
constexpr unsigned HashStartValue = 0x9E3779B9U;
constexpr unsigned HashMask = 0x00FFFFFFU;
constexpr unsigned HashAvoidZero = 0x00800000U;

struct CountedEntry {
    RegistrableDomain key;
    std::uint32_t value;
};

static_assert(sizeof(CountedEntry) == 16);
static_assert(alignof(CountedEntry) == 8);
static_assert(offsetof(CountedEntry, key) == 0);
static_assert(offsetof(CountedEntry, value) == 8);

const String& EntryKey(const RegistrableDomain& entry) {
    return entry.string;
}

const String& EntryKey(const CountedEntry& entry) {
    return entry.key.string;
}

template<typename Value>
unsigned TableSize(const Value* table) {
    return reinterpret_cast<const unsigned*>(table)[-1];
}

template<typename Value>
unsigned TableSizeMask(const Value* table) {
    return reinterpret_cast<const unsigned*>(table)[-2];
}

template<typename Value>
unsigned TableKeyCount(const Value* table) {
    return reinterpret_cast<const unsigned*>(table)[-3];
}

template<typename Value>
unsigned TableDeletedCount(const Value* table) {
    return reinterpret_cast<const unsigned*>(table)[-4];
}

template<typename Value>
void SetTableSize(Value* table, unsigned size) {
    reinterpret_cast<unsigned*>(table)[-1] = size;
}

template<typename Value>
void SetTableSizeMask(Value* table, unsigned mask) {
    reinterpret_cast<unsigned*>(table)[-2] = mask;
}

template<typename Value>
void SetTableKeyCount(Value* table, unsigned count) {
    reinterpret_cast<unsigned*>(table)[-3] = count;
}

template<typename Value>
void SetTableDeletedCount(Value* table, unsigned count) {
    reinterpret_cast<unsigned*>(table)[-4] = count;
}

bool IsEmptyBucket(const RegistrableDomain& entry) {
    return !entry.string.impl;
}

bool IsDeletedBucket(const RegistrableDomain& entry) {
    return entry.string.impl == reinterpret_cast<const StringImpl*>(DeletedImplValue);
}

bool IsEmptyBucket(const CountedEntry& entry) {
    return IsEmptyBucket(entry.key);
}

bool IsDeletedBucket(const CountedEntry& entry) {
    return IsDeletedBucket(entry.key);
}

StringView ImplView(const StringImpl* impl) {
    return {impl->data, impl->length, !!(impl->hashAndFlags & String8BitFlag)};
}

std::uint16_t UnitAt(const StringView& view, unsigned index) {
    return view.is8Bit ? static_cast<const unsigned char*>(view.data)[index] : static_cast<const char16_t*>(view.data)[index];
}

std::uint16_t FoldUnit(std::uint16_t unit) {
    return (unit >= 'A' && unit <= 'Z') ? static_cast<std::uint16_t>(unit + 0x20) : unit;
}

unsigned DomainHash(const StringImpl* impl) {
    StringView view = ImplView(impl);
    unsigned result = HashStartValue;
    unsigned index = 0;
    unsigned pairs = view.length >> 1;
    bool remainder = view.length & 1;
    for (unsigned i = 0; i < pairs; ++i) {
        unsigned first = FoldUnit(UnitAt(view, index++));
        unsigned second = FoldUnit(UnitAt(view, index++));
        unsigned hashed = result + first;
        hashed = (hashed << 16) ^ ((second << 11) ^ hashed);
        hashed += hashed >> 11;
        result = hashed;
    }
    if (remainder) {
        result += FoldUnit(UnitAt(view, index));
        result ^= result << 11;
        result += result >> 17;
    }
    result ^= result << 3;
    result += result >> 5;
    result ^= result << 2;
    result += result >> 15;
    result ^= result << 10;
    result &= HashMask;
    return result ? result : HashAvoidZero;
}

bool DomainEqual(const StringImpl* a, const StringImpl* b) {
    if (!a || !b) return a == b;
    StringView first = ImplView(a);
    StringView second = ImplView(b);
    if (first.length != second.length) return false;
    for (unsigned index = 0; index < first.length; ++index) {
        if (FoldUnit(UnitAt(first, index)) != FoldUnit(UnitAt(second, index))) return false;
    }
    return true;
}

template<typename Value>
Value* AllocateTable(unsigned size) {
    auto* memory = _ZN3WTF16fastZeroedMallocEm(MetadataSize + static_cast<std::size_t>(size) * sizeof(Value));
    return reinterpret_cast<Value*>(static_cast<unsigned char*>(memory) + MetadataSize);
}

template<typename Value>
Value* ReinsertEntry(Value* table, Value&& entry) {
    unsigned mask = TableSizeMask(table);
    unsigned hash = DomainHash(EntryKey(entry).impl);
    unsigned index = hash & mask;
    unsigned probeCount = 0;
    while (true) {
        Value* candidate = table + index;
        if (IsEmptyBucket(*candidate)) {
            new (candidate) Value(std::move(entry));
            return candidate;
        }
        ++probeCount;
        index = (index + probeCount) & mask;
    }
}

template<typename Value>
Value* Rehash(Value*& table, unsigned newSize, Value* entry) {
    unsigned oldTableSize = table ? TableSize(table) : 0;
    Value* oldTable = table;
    unsigned oldKeyCount = table ? TableKeyCount(table) : 0;
    table = AllocateTable<Value>(newSize);
    SetTableSize(table, newSize);
    SetTableSizeMask(table, newSize - 1);
    SetTableDeletedCount(table, 0);
    SetTableKeyCount(table, oldKeyCount);
    Value* newEntry = nullptr;
    for (unsigned i = 0; i != oldTableSize; ++i) {
        Value& oldEntry = oldTable[i];
        if (IsEmptyBucket(oldEntry) || IsDeletedBucket(oldEntry)) continue;
        Value* reinserted = ReinsertEntry(table, std::move(oldEntry));
        if (&oldEntry == entry) newEntry = reinserted;
    }
    if (oldTable) _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(oldTable) - MetadataSize);
    return newEntry;
}

template<typename Value>
bool ShouldExpand(const Value* table) {
    std::uint64_t keyAndDeleteCount = static_cast<std::uint64_t>(TableKeyCount(table)) + TableDeletedCount(table);
    std::uint64_t size = TableSize(table);
    if (size <= MaxSmallTableCapacity) return keyAndDeleteCount * SmallMaxLoadDenominator >= size * SmallMaxLoadNumerator;
    return keyAndDeleteCount * LargeMaxLoadDenominator >= size * LargeMaxLoadNumerator;
}

template<typename Value>
Value* Expand(Value*& table, Value* entry = nullptr) {
    unsigned oldSize = table ? TableSize(table) : 0;
    unsigned newSize;
    if (!oldSize) newSize = MinimumTableSize;
    else if (static_cast<std::uint64_t>(TableKeyCount(table)) * MinLoad < static_cast<std::uint64_t>(TableSize(table)) * 2) newSize = oldSize;
    else newSize = oldSize * 2;
    return Rehash(table, newSize, entry);
}

template<typename Value>
void AddEntry(Value*& table, Value&& entry) {
    if (!table) Expand(table);
    unsigned mask = TableSizeMask(table);
    unsigned hash = DomainHash(EntryKey(entry).impl);
    unsigned index = hash & mask;
    unsigned probeCount = 0;
    Value* deletedEntry = nullptr;
    Value* entrySlot = nullptr;
    while (true) {
        entrySlot = table + index;
        if (IsEmptyBucket(*entrySlot)) break;
        if (IsDeletedBucket(*entrySlot)) deletedEntry = entrySlot;
        else if (DomainEqual(EntryKey(*entrySlot).impl, EntryKey(entry).impl)) return;
        ++probeCount;
        index = (index + probeCount) & mask;
    }
    if (deletedEntry) {
        std::memset(deletedEntry, 0, sizeof(Value));
        entrySlot = deletedEntry;
        SetTableDeletedCount(table, TableDeletedCount(table) - 1);
    }
    new (entrySlot) Value(std::move(entry));
    SetTableKeyCount(table, TableKeyCount(table) + 1);
    if (ShouldExpand(table)) Expand(table, entrySlot);
}

template<typename Value, typename Function>
void IterateTable(const Value* table, Function&& function) {
    if (!table) return;
    unsigned size = TableSize(table);
    for (unsigned i = 0; i < size; ++i) {
        const Value& entry = table[i];
        if (!IsEmptyBucket(entry) && !IsDeletedBucket(entry)) function(entry);
    }
}

template<typename Value>
void DestroyTable(Value* table) {
    if (!table) return;
    unsigned size = TableSize(table);
    for (unsigned i = 0; i < size; ++i) {
        if (!IsDeletedBucket(table[i])) NKWebKit::WtfAbi::Release(EntryKey(table[i]).impl);
    }
    _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(table) - MetadataSize);
}

String MakeString(const char* literal, unsigned length) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(literal), length);
}

template<std::size_t N>
String MakeString(const char (&literal)[N]) {
    return MakeString(literal, static_cast<unsigned>(N - 1));
}

RegistrableDomain MakeRegistrableDomain(const String& domain) {
    RegistrableDomain result{String{domain}};
    if (!result.string.impl || !result.string.impl->length) result.string = MakeString("nullOrigin", 10);
    return result;
}

ResourceLoadStatistics* Checked(ResourceLoadStatistics* self, const char* operation) {
    if (!self) throw std::invalid_argument(std::string("WebCore::ResourceLoadStatistics::") + operation + ": null this");
    return self;
}

const ResourceLoadStatistics& CheckedSource(const ResourceLoadStatistics* other, const char* operation) {
    if (!other) throw std::invalid_argument(std::string("WebCore::ResourceLoadStatistics::") + operation + ": null source");
    return *other;
}

const KeyedEncoderVtable* CheckedVtable(const KeyedEncoder* encoder, const char* operation) {
    if (!encoder) throw std::invalid_argument(std::string("WebCore::ResourceLoadStatistics::") + operation + ": null encoder");
    if (!encoder->vtable) throw std::invalid_argument(std::string("WebCore::ResourceLoadStatistics::") + operation + ": encoder without vtable");
    return encoder->vtable;
}

const KeyedDecoderVtable* CheckedVtable(const KeyedDecoder* decoder, const char* operation) {
    if (!decoder) throw std::invalid_argument(std::string("WebCore::ResourceLoadStatistics::") + operation + ": null decoder");
    if (!decoder->vtable) throw std::invalid_argument(std::string("WebCore::ResourceLoadStatistics::") + operation + ": decoder without vtable");
    return decoder->vtable;
}

void EncodeString(KeyedEncoder* encoder, const String& key, const String& value) {
    CheckedVtable(encoder, "encode")->encodeString(encoder, &key, &value);
}

void EncodeDouble(KeyedEncoder* encoder, const String& key, double value) {
    CheckedVtable(encoder, "encode")->encodeDouble(encoder, &key, value);
}

void EncodeBool(KeyedEncoder* encoder, const String& key, bool value) {
    CheckedVtable(encoder, "encode")->encodeBool(encoder, &key, value);
}

void EncodeUInt32(KeyedEncoder* encoder, const String& key, std::uint32_t value) {
    CheckedVtable(encoder, "encode")->encodeUInt32(encoder, &key, value);
}

void BeginArray(KeyedEncoder* encoder, const String& key) {
    CheckedVtable(encoder, "encode")->beginArray(encoder, &key);
}

void BeginArrayElement(KeyedEncoder* encoder) {
    CheckedVtable(encoder, "encode")->beginArrayElement(encoder);
}

void EndArrayElement(KeyedEncoder* encoder) {
    CheckedVtable(encoder, "encode")->endArrayElement(encoder);
}

void EndArray(KeyedEncoder* encoder) {
    CheckedVtable(encoder, "encode")->endArray(encoder);
}

template<std::size_t LabelLength, std::size_t KeyLength>
void EncodeDomainSet(KeyedEncoder* encoder, const char (&label)[LabelLength], const char (&key)[KeyLength], const void* set) {
    auto* table = static_cast<const RegistrableDomain*>(set);
    if (!table || !TableKeyCount(table)) return;
    String labelString = MakeString(label);
    String keyString = MakeString(key);
    BeginArray(encoder, std::move(labelString));
    IterateTable(table, [&](const RegistrableDomain& entry) {
        BeginArrayElement(encoder);
        EncodeString(encoder, keyString, entry.string);
        EndArrayElement(encoder);
    });
    EndArray(encoder);
}

bool DecodeString(KeyedDecoder* decoder, const String& key, String& result) {
    return CheckedVtable(decoder, "decode")->decodeString(decoder, &key, &result);
}

bool DecodeBool(KeyedDecoder* decoder, const String& key, bool& result) {
    return CheckedVtable(decoder, "decode")->decodeBool(decoder, &key, &result);
}

bool DecodeUInt32(KeyedDecoder* decoder, const String& key, std::uint32_t& result) {
    return CheckedVtable(decoder, "decode")->decodeUInt32(decoder, &key, &result);
}

bool DecodeDouble(KeyedDecoder* decoder, const String& key, double& result) {
    return CheckedVtable(decoder, "decode")->decodeDouble(decoder, &key, &result);
}

bool BeginArray(KeyedDecoder* decoder, const String& key) {
    return CheckedVtable(decoder, "decode")->beginArray(decoder, &key);
}

bool BeginArrayElement(KeyedDecoder* decoder) {
    return CheckedVtable(decoder, "decode")->beginArrayElement(decoder);
}

void EndArrayElement(KeyedDecoder* decoder) {
    CheckedVtable(decoder, "decode")->endArrayElement(decoder);
}

void EndArray(KeyedDecoder* decoder) {
    CheckedVtable(decoder, "decode")->endArray(decoder);
}

template<typename Value, std::size_t LabelLength, std::size_t KeyLength>
void DecodeDomainSet(KeyedDecoder* decoder, const char (&label)[LabelLength], const char (&key)[KeyLength], Value*& set) {
    if (!BeginArray(decoder, MakeString(label))) return;
    String keyString = MakeString(key);
    while (BeginArrayElement(decoder)) {
        String element;
        if (!DecodeString(decoder, keyString, element)) {
            EndArrayElement(decoder);
            break;
        }
        AddEntry(set, MakeRegistrableDomain(element));
        EndArrayElement(decoder);
    }
    EndArray(decoder);
}

template<typename Value, std::size_t LabelLength>
void DecodeCountedSetToSet(KeyedDecoder* decoder, const char (&label)[LabelLength], Value*& set) {
    CountedEntry* counted = nullptr;
    if (!BeginArray(decoder, MakeString(label))) return;
    String originKey = MakeString("origin");
    String countKey = MakeString("count");
    while (BeginArrayElement(decoder)) {
        String domain;
        if (!DecodeString(decoder, originKey, domain)) {
            EndArrayElement(decoder);
            break;
        }
        std::uint32_t count;
        if (!DecodeUInt32(decoder, countKey, count)) {
            EndArrayElement(decoder);
            break;
        }
        AddEntry(counted, CountedEntry{MakeRegistrableDomain(domain), count});
        EndArrayElement(decoder);
    }
    EndArray(decoder);
    IterateTable(counted, [&](const CountedEntry& entry) {
        AddEntry(set, RegistrableDomain{entry.key});
    });
    DestroyTable(counted);
}

void MergeSet(void*& to, const void* from) {
    auto* fromTable = static_cast<const RegistrableDomain*>(from);
    IterateTable(fromTable, [&](const RegistrableDomain& entry) {
        AddEntry(reinterpret_cast<RegistrableDomain*&>(to), RegistrableDomain{entry});
    });
}

template<std::size_t LabelLength>
void AppendBoolean(StringBuilder& builder, const char (&label)[LabelLength], bool flag) {
    NKWebKit::WtfAbi::AppendCString(builder, "    ");
    NKWebKit::WtfAbi::AppendCString(builder, label);
    NKWebKit::WtfAbi::AppendCString(builder, ": ");
    NKWebKit::WtfAbi::AppendCString(builder, flag ? "Yes" : "No");
}

template<std::size_t LabelLength>
void AppendDomainSet(StringBuilder& builder, const char (&label)[LabelLength], const void* set) {
    auto* table = static_cast<const RegistrableDomain*>(set);
    if (!table || !TableKeyCount(table)) return;
    NKWebKit::WtfAbi::AppendCString(builder, "    ");
    NKWebKit::WtfAbi::AppendCString(builder, label);
    NKWebKit::WtfAbi::AppendCString(builder, ":\n");
    IterateTable(table, [&](const RegistrableDomain& entry) {
        NKWebKit::WtfAbi::AppendCString(builder, "        ");
        NKWebKit::WtfAbi::AppendString(builder, entry.string.impl);
        NKWebKit::WtfAbi::AppendCharacter(builder, '\n');
    });
}

bool HasHadRecentUserInteraction(double interactionTimeSeconds) {
    return interactionTimeSeconds > 0 && _ZN3WTF8WallTime3nowEv().m_value - interactionTimeSeconds < 86400.0;
}

void Merge(ResourceLoadStatistics* self, const ResourceLoadStatistics& other) {
    if (self->lastSeen.m_value < other.lastSeen.m_value) self->lastSeen = other.lastSeen;
    if (!other.hadUserInteraction) {
        if (!other.mostRecentUserInteractionTime.m_value) {
            self->hadUserInteraction = false;
            self->mostRecentUserInteractionTime = WTF::WallTime{};
        }
    } else {
        self->hadUserInteraction = true;
        if (self->mostRecentUserInteractionTime.m_value < other.mostRecentUserInteractionTime.m_value)
            self->mostRecentUserInteractionTime = other.mostRecentUserInteractionTime;
    }
    self->grandfathered |= other.grandfathered;
    MergeSet(self->storageAccessUnderTopFrameDomains, other.storageAccessUnderTopFrameDomains);
    MergeSet(self->topFrameUniqueRedirectsTo, other.topFrameUniqueRedirectsTo);
    MergeSet(self->topFrameUniqueRedirectsFrom, other.topFrameUniqueRedirectsFrom);
    MergeSet(self->topFrameLinkDecorationsFrom, other.topFrameLinkDecorationsFrom);
    self->gotLinkDecorationFromPrevalentResource |= other.gotLinkDecorationFromPrevalentResource;
    MergeSet(self->topFrameLoadedThirdPartyScripts, other.topFrameLoadedThirdPartyScripts);
    MergeSet(self->subframeUnderTopFrameDomains, other.subframeUnderTopFrameDomains);
    MergeSet(self->subresourceUnderTopFrameDomains, other.subresourceUnderTopFrameDomains);
    MergeSet(self->subresourceUniqueRedirectsTo, other.subresourceUniqueRedirectsTo);
    MergeSet(self->subresourceUniqueRedirectsFrom, other.subresourceUniqueRedirectsFrom);
    self->isPrevalentResource |= other.isPrevalentResource;
    self->isVeryPrevalentResource |= other.isVeryPrevalentResource;
    self->dataRecordsRemoved = std::max(self->dataRecordsRemoved, other.dataRecordsRemoved);
}

void Encode(const ResourceLoadStatistics* self, KeyedEncoder* encoder) {
    EncodeString(encoder, MakeString("PrevalentResourceDomain"), self->registrableDomain.string);
    EncodeDouble(encoder, MakeString("lastSeen"), self->lastSeen.m_value);
    EncodeBool(encoder, MakeString("hadUserInteraction"), self->hadUserInteraction);
    EncodeDouble(encoder, MakeString("mostRecentUserInteraction"), self->mostRecentUserInteractionTime.m_value);
    EncodeBool(encoder, MakeString("grandfathered"), self->grandfathered);
    EncodeDomainSet(encoder, "storageAccessUnderTopFrameDomains", "domain", self->storageAccessUnderTopFrameDomains);
    EncodeDomainSet(encoder, "topFrameUniqueRedirectsTo", "domain", self->topFrameUniqueRedirectsTo);
    EncodeDomainSet(encoder, "topFrameUniqueRedirectsFrom", "domain", self->topFrameUniqueRedirectsFrom);
    EncodeDomainSet(encoder, "topFrameLinkDecorationsFrom", "domain", self->topFrameLinkDecorationsFrom);
    EncodeBool(encoder, MakeString("gotLinkDecorationFromPrevalentResource"), self->gotLinkDecorationFromPrevalentResource);
    EncodeDomainSet(encoder, "topFrameLoadedThirdPartyScripts", "domain", self->topFrameLoadedThirdPartyScripts);
    EncodeDomainSet(encoder, "subframeUnderTopFrameDomains", "domain", self->subframeUnderTopFrameDomains);
    EncodeDomainSet(encoder, "subresourceUnderTopFrameDomains", "domain", self->subresourceUnderTopFrameDomains);
    EncodeDomainSet(encoder, "subresourceUniqueRedirectsTo", "domain", self->subresourceUniqueRedirectsTo);
    EncodeDomainSet(encoder, "subresourceUniqueRedirectsFrom", "domain", self->subresourceUniqueRedirectsFrom);
    EncodeBool(encoder, MakeString("isPrevalentResource"), self->isPrevalentResource);
    EncodeBool(encoder, MakeString("isVeryPrevalentResource"), self->isVeryPrevalentResource);
    EncodeUInt32(encoder, MakeString("dataRecordsRemoved"), self->dataRecordsRemoved);
    EncodeUInt32(encoder, MakeString("timesAccessedAsFirstPartyDueToUserInteraction"), self->timesAccessedAsFirstPartyDueToUserInteraction);
    EncodeUInt32(encoder, MakeString("timesAccessedAsFirstPartyDueToStorageAccessAPI"), self->timesAccessedAsFirstPartyDueToStorageAccessAPI);
}

bool Decode(ResourceLoadStatistics* self, KeyedDecoder* decoder, unsigned modelVersion) {
    String domainString;
    if (modelVersion >= 15) {
        if (!DecodeString(decoder, MakeString("PrevalentResourceDomain"), domainString)) return false;
    } else {
        if (!DecodeString(decoder, MakeString("PrevalentResourceOrigin"), domainString)) return false;
    }
    self->registrableDomain = MakeRegistrableDomain(domainString);

    if (!DecodeBool(decoder, MakeString("hadUserInteraction"), self->hadUserInteraction)) return false;

    if (modelVersion >= 15)
        DecodeDomainSet(decoder, "storageAccessUnderTopFrameDomains", "domain", reinterpret_cast<RegistrableDomain*&>(self->storageAccessUnderTopFrameDomains));
    else
        DecodeDomainSet(decoder, "storageAccessUnderTopFrameOrigins", "origin", reinterpret_cast<RegistrableDomain*&>(self->storageAccessUnderTopFrameDomains));

    if (modelVersion >= 15) {
        DecodeDomainSet(decoder, "topFrameUniqueRedirectsTo", "domain", reinterpret_cast<RegistrableDomain*&>(self->topFrameUniqueRedirectsTo));
        DecodeDomainSet(decoder, "topFrameUniqueRedirectsFrom", "domain", reinterpret_cast<RegistrableDomain*&>(self->topFrameUniqueRedirectsFrom));
    } else if (modelVersion >= 11) {
        DecodeCountedSetToSet(decoder, "topFrameUniqueRedirectsTo", reinterpret_cast<RegistrableDomain*&>(self->topFrameUniqueRedirectsTo));
        DecodeCountedSetToSet(decoder, "topFrameUniqueRedirectsFrom", reinterpret_cast<RegistrableDomain*&>(self->topFrameUniqueRedirectsFrom));
    }

    if (modelVersion >= 16) {
        DecodeDomainSet(decoder, "topFrameLinkDecorationsFrom", "domain", reinterpret_cast<RegistrableDomain*&>(self->topFrameLinkDecorationsFrom));
        if (!DecodeBool(decoder, MakeString("gotLinkDecorationFromPrevalentResource"), self->gotLinkDecorationFromPrevalentResource)) return false;
    }

    if (modelVersion >= 17)
        DecodeCountedSetToSet(decoder, "topFrameLoadedThirdPartyScripts", reinterpret_cast<RegistrableDomain*&>(self->topFrameLoadedThirdPartyScripts));

    if (modelVersion >= 15)
        DecodeDomainSet(decoder, "subframeUnderTopFrameDomains", "domain", reinterpret_cast<RegistrableDomain*&>(self->subframeUnderTopFrameDomains));
    else if (modelVersion >= 14)
        DecodeCountedSetToSet(decoder, "subframeUnderTopFrameOrigins", reinterpret_cast<RegistrableDomain*&>(self->subframeUnderTopFrameDomains));

    if (modelVersion >= 15) {
        DecodeDomainSet(decoder, "subresourceUnderTopFrameDomains", "domain", reinterpret_cast<RegistrableDomain*&>(self->subresourceUnderTopFrameDomains));
        DecodeDomainSet(decoder, "subresourceUniqueRedirectsTo", "domain", reinterpret_cast<RegistrableDomain*&>(self->subresourceUniqueRedirectsTo));
        DecodeDomainSet(decoder, "subresourceUniqueRedirectsFrom", "domain", reinterpret_cast<RegistrableDomain*&>(self->subresourceUniqueRedirectsFrom));
    } else {
        DecodeCountedSetToSet(decoder, "subresourceUnderTopFrameOrigins", reinterpret_cast<RegistrableDomain*&>(self->subresourceUnderTopFrameDomains));
        DecodeCountedSetToSet(decoder, "subresourceUniqueRedirectsTo", reinterpret_cast<RegistrableDomain*&>(self->subresourceUniqueRedirectsTo));
        if (modelVersion >= 11)
            DecodeCountedSetToSet(decoder, "subresourceUniqueRedirectsFrom", reinterpret_cast<RegistrableDomain*&>(self->subresourceUniqueRedirectsFrom));
    }

    if (!DecodeBool(decoder, MakeString("isPrevalentResource"), self->isPrevalentResource)) return false;

    if (modelVersion >= 12) {
        if (!DecodeBool(decoder, MakeString("isVeryPrevalentResource"), self->isVeryPrevalentResource)) return false;
    }

    if (modelVersion < 14) {
        self->isPrevalentResource = false;
        self->isVeryPrevalentResource = false;
    }

    if (!DecodeUInt32(decoder, MakeString("dataRecordsRemoved"), self->dataRecordsRemoved)) return false;

    double mostRecentUserInteractionTimeAsDouble;
    if (!DecodeDouble(decoder, MakeString("mostRecentUserInteraction"), mostRecentUserInteractionTimeAsDouble)) return false;
    self->mostRecentUserInteractionTime = WTF::WallTime{mostRecentUserInteractionTimeAsDouble};

    if (!DecodeBool(decoder, MakeString("grandfathered"), self->grandfathered)) return false;

    double lastSeenTimeAsDouble;
    if (!DecodeDouble(decoder, MakeString("lastSeen"), lastSeenTimeAsDouble)) return false;
    self->lastSeen = WTF::WallTime{lastSeenTimeAsDouble};

    if (modelVersion >= 11) {
        if (!DecodeUInt32(decoder, MakeString("timesAccessedAsFirstPartyDueToUserInteraction"), self->timesAccessedAsFirstPartyDueToUserInteraction))
            self->timesAccessedAsFirstPartyDueToUserInteraction = 0;
        if (!DecodeUInt32(decoder, MakeString("timesAccessedAsFirstPartyDueToStorageAccessAPI"), self->timesAccessedAsFirstPartyDueToStorageAccessAPI))
            self->timesAccessedAsFirstPartyDueToStorageAccessAPI = 0;
    }

    return true;
}

String ToString(const ResourceLoadStatistics* self) {
    StringBuilder builder{};
    builder.shouldCrashOnOverflow = true;
    builder.is8Bit = true;
    NKWebKit::WtfAbi::AppendCString(builder, "Registrable domain: ");
    NKWebKit::WtfAbi::AppendString(builder, self->registrableDomain.string.impl);
    NKWebKit::WtfAbi::AppendCharacter(builder, '\n');
    AppendBoolean(builder, "hadUserInteraction", self->hadUserInteraction);
    NKWebKit::WtfAbi::AppendCharacter(builder, '\n');
    NKWebKit::WtfAbi::AppendCString(builder, "    mostRecentUserInteraction: ");
    NKWebKit::WtfAbi::AppendCString(builder, HasHadRecentUserInteraction(self->mostRecentUserInteractionTime.m_value) ? "within 24 hours" : "-1");
    NKWebKit::WtfAbi::AppendCharacter(builder, '\n');
    AppendBoolean(builder, "grandfathered", self->grandfathered);
    NKWebKit::WtfAbi::AppendCharacter(builder, '\n');
    AppendDomainSet(builder, "storageAccessUnderTopFrameDomains", self->storageAccessUnderTopFrameDomains);
    AppendDomainSet(builder, "topFrameUniqueRedirectsTo", self->topFrameUniqueRedirectsTo);
    AppendDomainSet(builder, "topFrameUniqueRedirectsFrom", self->topFrameUniqueRedirectsFrom);
    AppendDomainSet(builder, "topFrameLinkDecorationsFrom", self->topFrameLinkDecorationsFrom);
    AppendBoolean(builder, "gotLinkDecorationFromPrevalentResource", self->gotLinkDecorationFromPrevalentResource);
    NKWebKit::WtfAbi::AppendCharacter(builder, '\n');
    AppendDomainSet(builder, "topFrameLoadedThirdPartyScripts", self->topFrameLoadedThirdPartyScripts);
    AppendDomainSet(builder, "subframeUnderTopFrameDomains", self->subframeUnderTopFrameDomains);
    AppendDomainSet(builder, "subresourceUnderTopFrameDomains", self->subresourceUnderTopFrameDomains);
    AppendDomainSet(builder, "subresourceUniqueRedirectsTo", self->subresourceUniqueRedirectsTo);
    AppendDomainSet(builder, "subresourceUniqueRedirectsFrom", self->subresourceUniqueRedirectsFrom);
    AppendBoolean(builder, "isPrevalentResource", self->isPrevalentResource);
    NKWebKit::WtfAbi::AppendCharacter(builder, '\n');
    AppendBoolean(builder, "isVeryPrevalentResource", self->isVeryPrevalentResource);
    NKWebKit::WtfAbi::AppendCharacter(builder, '\n');
    NKWebKit::WtfAbi::AppendCString(builder, "    dataRecordsRemoved: ");
    String number = _ZN3WTF6String6numberEj(self->dataRecordsRemoved);
    NKWebKit::WtfAbi::AppendString(builder, number.impl);
    NKWebKit::WtfAbi::AppendCharacter(builder, '\n');
    String result = NKWebKit::WtfAbi::ToString(builder);
    NKWebKit::WtfAbi::Clear(builder);
    return result;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore22ResourceLoadStatistics5mergeERKS0_(ResourceLoadStatistics* self, const ResourceLoadStatistics* other) {
    Merge(Checked(self, "merge"), CheckedSource(other, "merge"));
}

bool APS5_VABI _ZN7WebCore22ResourceLoadStatistics6decodeERNS_12KeyedDecoderEj(ResourceLoadStatistics* self, KeyedDecoder* decoder, unsigned modelVersion) {
    return Decode(Checked(self, "decode"), decoder, modelVersion);
}

void APS5_VABI _ZNK7WebCore22ResourceLoadStatistics6encodeERNS_12KeyedEncoderE(const ResourceLoadStatistics* self, KeyedEncoder* encoder) {
    Checked(const_cast<ResourceLoadStatistics*>(self), "encode");
    CheckedVtable(encoder, "encode");
    Encode(self, encoder);
}

NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore22ResourceLoadStatistics8toStringEv(const ResourceLoadStatistics* self) {
    return ToString(Checked(const_cast<ResourceLoadStatistics*>(self), "toString"));
}

}
