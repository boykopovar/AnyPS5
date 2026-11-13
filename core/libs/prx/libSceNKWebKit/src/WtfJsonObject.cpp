// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/JSONValues.cpp
/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington. All rights reserved.
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Derived from Sony PS5 WebKit 11.x revision 80f77e94c6, Source/WTF/wtf/HashTable.h
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
#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/include/JsonExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace {

using NKWebKit::JsonAbi::Array;
using NKWebKit::JsonAbi::Deref;
using NKWebKit::JsonAbi::Object;
using NKWebKit::JsonAbi::ObjectBase;
using NKWebKit::JsonAbi::Ref;
using NKWebKit::JsonAbi::RefPtr;
using NKWebKit::JsonAbi::StringBuilder;
using NKWebKit::JsonAbi::StringValueMap;
using NKWebKit::JsonAbi::StringValuePair;
using NKWebKit::JsonAbi::Value;
using NKWebKit::JsonAbi::ValueType;
using NKWebKit::JsonAbi::ValueVtableStorage;
using NKWebKit::JsonAbi::Vector;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static_assert(sizeof(Vector<String>) == 16);
static_assert(offsetof(Vector<String>, buffer) == 0);
static_assert(offsetof(Vector<String>, capacity) == 8);
static_assert(offsetof(Vector<String>, size) == 12);

constexpr std::size_t MetadataSize = 4 * sizeof(unsigned);
constexpr unsigned MinimumTableSize = 8;
constexpr unsigned MinLoad = 6;
constexpr int TableSizeOffset = -1;
constexpr int TableSizeMaskOffset = -2;
constexpr int KeyCountOffset = -3;
constexpr int DeletedCountOffset = -4;
constexpr unsigned HashFlagCount = 8;
constexpr unsigned Is8BitFlag = 1U << 2;

template<typename T>
T& Checked(T* ptr) {
    if (!ptr) throw std::invalid_argument("null WTF JSON object");
    return *ptr;
}

StringImpl* DeletedKey() {
    return reinterpret_cast<StringImpl*>(~std::uintptr_t(0));
}

unsigned* Metadata(StringValuePair* table) {
    return reinterpret_cast<unsigned*>(table);
}

bool IsEmptyBucket(const StringValuePair& bucket) {
    return !bucket.key.impl;
}

bool IsDeletedBucket(const StringValuePair& bucket) {
    return bucket.key.impl == DeletedKey();
}

unsigned KeyHash(const StringImpl* key) {
    unsigned hash = key->hashAndFlags >> HashFlagCount;
    return hash ? hash : _ZNK3WTF10StringImpl12hashSlowCaseEv(key);
}

std::size_t KeySizeInBytes(const StringImpl* key) {
    return std::size_t(key->length) * (key->hashAndFlags & Is8BitFlag ? 1U : 2U);
}

StringValuePair* Find(const StringValueMap& map, const String& name) {
    StringValuePair* table = map.table;
    if (!table) return nullptr;
    if (!name.impl) throw std::invalid_argument("null WTF JSON object key");
    unsigned sizeMask = Metadata(table)[TableSizeMaskOffset];
    unsigned i = KeyHash(name.impl) & sizeMask;
    unsigned probeCount = 0;
    while (true) {
        StringValuePair* entry = table + i;
        if (IsEmptyBucket(*entry)) return nullptr;
        if (!IsDeletedBucket(*entry) && _ZN3WTF5equalEPKNS_10StringImplES2_(entry->key.impl, name.impl)) return entry;
        ++probeCount;
        i = (i + probeCount) & sizeMask;
    }
}

void Rehash(StringValueMap& map, unsigned newTableSize) {
    StringValuePair* oldTable = map.table;
    unsigned oldTableSize = Metadata(oldTable)[TableSizeOffset];
    unsigned keyCount = Metadata(oldTable)[KeyCountOffset];
    auto* storage = static_cast<unsigned char*>(_ZN3WTF16fastZeroedMallocEm(MetadataSize + std::size_t(newTableSize) * sizeof(StringValuePair)));
    auto* table = reinterpret_cast<StringValuePair*>(storage + MetadataSize);
    Metadata(table)[TableSizeOffset] = newTableSize;
    Metadata(table)[TableSizeMaskOffset] = newTableSize - 1U;
    Metadata(table)[DeletedCountOffset] = 0U;
    Metadata(table)[KeyCountOffset] = keyCount;
    unsigned sizeMask = newTableSize - 1U;
    for (unsigned j = 0U; j < oldTableSize; ++j) {
        StringValuePair& oldEntry = oldTable[j];
        if (IsEmptyBucket(oldEntry) || IsDeletedBucket(oldEntry)) continue;
        unsigned i = KeyHash(oldEntry.key.impl) & sizeMask;
        unsigned probeCount = 0;
        while (!IsEmptyBucket(table[i])) {
            ++probeCount;
            i = (i + probeCount) & sizeMask;
        }
        table[i].key.impl = oldEntry.key.impl;
        table[i].value.ptr = oldEntry.value.ptr;
        oldEntry.key.impl = nullptr;
        oldEntry.value.ptr = nullptr;
    }
    _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(oldTable) - MetadataSize);
    map.table = table;
}

void RemoveFromMap(StringValueMap& map, const String& name) {
    StringValuePair* entry = Find(map, name);
    if (!entry) return;
    StringImpl* key = entry->key.impl;
    entry->key.impl = DeletedKey();
    NKWebKit::WtfAbi::Release(key);
    Deref(entry->value.Leak());
    unsigned* metadata = Metadata(map.table);
    metadata[DeletedCountOffset] += 1U;
    metadata[KeyCountOffset] -= 1U;
    unsigned tableSize = metadata[TableSizeOffset];
    if (metadata[KeyCountOffset] * MinLoad < tableSize && tableSize > MinimumTableSize) Rehash(map, tableSize / 2U);
}

void RemoveFirstFromOrder(Vector<String>& order, const String& name) {
    for (unsigned i = 0U; i < order.size; ++i) {
        if (!_ZN3WTF5equalEPKNS_10StringImplES2_(order.buffer[i].impl, name.impl)) continue;
        StringImpl* removed = order.buffer[i].impl;
        std::memmove(static_cast<void*>(order.buffer + i), static_cast<const void*>(order.buffer + i + 1), std::size_t(order.size - i - 1U) * sizeof(String));
        --order.size;
        order.buffer[order.size].impl = nullptr;
        NKWebKit::WtfAbi::Release(removed);
        return;
    }
}

void DestroyOrder(Vector<String>& order) {
    for (unsigned i = 0U; i < order.size; ++i) {
        NKWebKit::WtfAbi::Release(std::exchange(order.buffer[i].impl, nullptr));
    }
    if (order.buffer) _ZN3WTF8fastFreeEPv(order.buffer);
    order.buffer = nullptr;
    order.capacity = 0U;
    order.size = 0U;
}

void DestroyMap(StringValueMap& map) {
    StringValuePair* table = map.table;
    if (!table) return;
    unsigned tableSize = Metadata(table)[TableSizeOffset];
    for (unsigned i = 0U; i < tableSize; ++i) {
        StringValuePair& bucket = table[i];
        if (IsDeletedBucket(bucket)) continue;
        Deref(bucket.value.Leak());
        NKWebKit::WtfAbi::Release(std::exchange(bucket.key.impl, nullptr));
    }
    _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(table) - MetadataSize);
    map.table = nullptr;
}

extern const ValueVtableStorage ObjectVtableInstance;

void APS5_VABI ObjectVtableD1(Value* self) {
    auto& object = static_cast<ObjectBase&>(Checked(self));
    object.vptr = &ObjectVtableInstance.dispatch;
    DestroyOrder(object.order);
    DestroyMap(object.map);
    _ZN3WTF8JSONImpl5ValueD2Ev(&object);
}

void APS5_VABI ObjectVtableD0(Value* self) {
    ObjectVtableD1(self);
    _ZN3WTF8fastFreeEPv(self);
}

RefPtr<Object> APS5_VABI ObjectAsObject(Value* self) {
    return RefPtr<Object>::Share(static_cast<Object*>(static_cast<ObjectBase*>(&Checked(self))));
}

RefPtr<const Object> APS5_VABI ObjectAsObjectConst(const Value* self) {
    return RefPtr<const Object>::Share(static_cast<const Object*>(static_cast<const ObjectBase*>(&Checked(self))));
}

RefPtr<Array> APS5_VABI ObjectAsArray(Value* self) {
    Checked(self);
    return RefPtr<Array>(nullptr);
}

void APS5_VABI ObjectWriteJSON(const Value*, StringBuilder&) {
    NotImplemented_nid_no_patch(__func__);
}

std::size_t APS5_VABI ObjectVtableMemoryCost(const Value* self) {
    return _ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(static_cast<const ObjectBase*>(self));
}

const ValueVtableStorage ObjectVtableInstance = {
    0,
    nullptr,
    {
        &ObjectVtableD1,
        &ObjectVtableD0,
        &ObjectAsObject,
        &ObjectAsObjectConst,
        &ObjectAsArray,
        &ObjectWriteJSON,
        &ObjectVtableMemoryCost,
    },
};

}  // namespace

Ref<Object> APS5_VABI _ZN3WTF8JSONImpl6Object6createEv() {
    auto* self = static_cast<Object*>(_ZN3WTF10fastMallocEm(sizeof(Object)));
    _ZN3WTF8JSONImpl5ValueC2ENS1_4TypeE(self, ValueType::Object);
    self->vptr = &ObjectVtableInstance.dispatch;
    self->map.table = nullptr;
    self->order.buffer = nullptr;
    self->order.capacity = 0U;
    self->order.size = 0U;
    return Ref<Object>::Adopt(self);
}

void APS5_VABI _ZN3WTF8JSONImpl10ObjectBase6removeERKNS_6StringE(ObjectBase* self, const String& name) {
    auto& object = Checked(self);
    RemoveFromMap(object.map, name);
    RemoveFirstFromOrder(object.order, name);
}

bool APS5_VABI _ZNK3WTF8JSONImpl10ObjectBase9getStringERKNS_6StringERS2_(const ObjectBase* self, const String& name, String& output) {
    const StringValuePair* entry = Find(Checked(self).map, name);
    if (!entry) return false;
    const Value& value = Checked(entry->value.Get());
    if (value.valueType != ValueType::String || !value.data.string) return false;
    value.data.string->refCount += 2U;
    output = String(value.data.string);
    return true;
}

std::size_t APS5_VABI _ZNK3WTF8JSONImpl10ObjectBase10memoryCostEv(const ObjectBase* self) {
    const auto& object = Checked(self);
    std::size_t cost = _ZNK3WTF8JSONImpl5Value10memoryCostEv(&object);
    StringValuePair* table = object.map.table;
    if (!table) return cost;
    unsigned tableSize = Metadata(table)[TableSizeOffset];
    for (unsigned i = 0U; i < tableSize; ++i) {
        const StringValuePair& bucket = table[i];
        if (IsEmptyBucket(bucket) || IsDeletedBucket(bucket)) continue;
        const Value& value = Checked(bucket.value.Get());
        cost += KeySizeInBytes(bucket.key.impl) + value.vptr->memoryCost(&value);
    }
    return cost;
}
