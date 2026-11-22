// Derived from Source/WebCore/storage/StorageMap.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2008-2021 Apple Inc. All Rights Reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/StorageMapExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::StorageMap;
using NKWebKit::WebCoreAbi::StorageMapImpl;
using NKWebKit::WebCoreAbi::StorageMapNoQuota;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringHashMapAdvance;
using NKWebKit::WtfAbi::StringHashMapAssign;
using NKWebKit::WtfAbi::StringHashMapBegin;
using NKWebKit::WtfAbi::StringHashMapEnd;
using NKWebKit::WtfAbi::StringHashMapFind;
using NKWebKit::WtfAbi::StringHashMapKeyCount;
using NKWebKit::WtfAbi::StringHashMapRemove;
using NKWebKit::WtfAbi::StringHashMapSet;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringPair;

struct CheckedSize {
    std::uint32_t value;
    bool overflowed;

    void Add(unsigned amount) {
        std::uint64_t sum = std::uint64_t(value) + amount;
        if (sum > std::numeric_limits<std::uint32_t>::max()) overflowed = true;
        value = static_cast<std::uint32_t>(sum);
    }

    void Subtract(unsigned amount) {
        if (amount > value) overflowed = true;
        value -= amount;
    }
};

struct QuotaRestore {
    unsigned& slot;
    unsigned saved;

    ~QuotaRestore() { slot = saved; }
};

StorageMap& Checked(StorageMap* self, const char* function) {
    if (!self || !self->impl) throw std::invalid_argument(std::string(function) + ": null StorageMap");
    return *self;
}

const StorageMap& Checked(const StorageMap* self, const char* function) {
    if (!self || !self->impl) throw std::invalid_argument(std::string(function) + ": null StorageMap");
    return *self;
}

const String& CheckedString(const String* string, const char* function) {
    if (!string) throw std::invalid_argument(std::string(function) + ": null String reference");
    return *string;
}

String Share(StringImpl* impl) {
    if (impl) impl->refCount += 2U;
    return String(impl);
}

unsigned SizeInBytes(const StringImpl* impl) {
    if (!impl) return 0U;
    const NKWebKit::WtfAbi::StringView view = NKWebKit::WtfAbi::ViewOf(impl);
    return static_cast<unsigned>(std::size_t(view.length) * (view.is8Bit ? 1U : 2U));
}

void InvalidateIterator(StorageMapImpl& impl) {
    impl.iterator = StringHashMapEnd(impl.map);
    impl.iteratorIndex = std::numeric_limits<unsigned>::max();
}

void SetIteratorToIndex(StorageMapImpl& impl, unsigned index) {
    if (impl.iteratorIndex == index) return;
    if (index < impl.iteratorIndex) {
        impl.iteratorIndex = 0U;
        impl.iterator = StringHashMapBegin(impl.map);
    }
    while (impl.iteratorIndex < index) {
        StringHashMapAdvance(impl.iterator);
        ++impl.iteratorIndex;
    }
}

void ReplaceWithCopy(StorageMap& map) {
    StorageMapImpl* copy = NKWebKit::WebCoreAbi::CreateStorageMapImpl();
    try {
        StringHashMapAssign(copy->map, map.impl->map);
    } catch (...) {
        NKWebKit::WebCoreAbi::DerefStorageMapImpl(copy);
        throw;
    }
    copy->currentSize = map.impl->currentSize;
    NKWebKit::WebCoreAbi::DerefStorageMapImpl(std::exchange(map.impl, copy));
}

}  // namespace

namespace NKWebKit::WebCoreAbi {

StorageMapImpl* CreateStorageMapImpl() {
    auto* impl = static_cast<StorageMapImpl*>(_ZN3WTF10fastMallocEm(sizeof(StorageMapImpl)));
    impl->refCount = 1U;
    impl->map.table = nullptr;
    impl->iterator = WtfAbi::StringHashMapEnd(impl->map);
    impl->iteratorIndex = std::numeric_limits<unsigned>::max();
    impl->currentSize = 0U;
    return impl;
}

void DerefStorageMapImpl(StorageMapImpl* impl) {
    if (!impl) return;
    unsigned next = impl->refCount - 1U;
    if (next) {
        impl->refCount = next;
        return;
    }
    WtfAbi::StringHashMapClear(impl->map);
    _ZN3WTF8fastFreeEPv(impl);
}

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

String APS5_VABI _ZN7WebCore10StorageMap3keyEj(StorageMap* self, unsigned index) {
    StorageMapImpl& impl = *Checked(self, __func__).impl;
    if (index >= StringHashMapKeyCount(impl.map)) return String();
    SetIteratorToIndex(impl, index);
    if (impl.iterator.position == impl.iterator.endPosition)
        throw std::logic_error(std::string(__func__) + ": iterator past the last entry");
    return Share(impl.iterator.position->key);
}

String APS5_VABI _ZNK7WebCore10StorageMap7getItemERKN3WTF6StringE(const StorageMap* self, const String* key) {
    const StorageMapImpl& impl = *Checked(self, __func__).impl;
    const StringPair* entry = StringHashMapFind(impl.map, CheckedString(key, __func__).impl);
    if (!entry) return String();
    return Share(entry->value);
}

void APS5_VABI _ZN7WebCore10StorageMap7setItemERKN3WTF6StringES4_RS2_Rb(StorageMap* self, const String* key, const String* value,
                                                                          String* oldValue, bool* quotaException) {
    StorageMap& map = Checked(self, __func__);
    CheckedString(key, __func__);
    CheckedString(value, __func__);
    if (!oldValue || !quotaException) throw std::invalid_argument(std::string(__func__) + ": null output reference");
    *quotaException = false;
    CheckedSize newSize{map.impl->currentSize, false};
    const StringPair* entry = StringHashMapFind(map.impl->map, key->impl);
    if (entry) {
        *oldValue = Share(entry->value);
        newSize.Subtract(SizeInBytes(oldValue->impl));
    } else {
        *oldValue = String();
        newSize.Add(SizeInBytes(key->impl));
    }
    newSize.Add(SizeInBytes(value->impl));
    if (map.quotaSize != StorageMapNoQuota && (newSize.overflowed || newSize.value > map.quotaSize)) {
        *quotaException = true;
        return;
    }
    if (newSize.overflowed) throw std::overflow_error(std::string(__func__) + ": the size of the map overflows 32 bits");
    if (!key->impl) throw std::invalid_argument(std::string(__func__) + ": null key");
    if (map.impl->refCount > 1U) ReplaceWithCopy(map);
    StringHashMapSet(map.impl->map, key->impl, value->impl);
    map.impl->currentSize = newSize.value;
    InvalidateIterator(*map.impl);
}

void APS5_VABI _ZN7WebCore10StorageMap20setItemIgnoringQuotaERKN3WTF6StringES4_(StorageMap* self, const String* key, const String* value) {
    StorageMap& map = Checked(self, __func__);
    QuotaRestore restore{map.quotaSize, map.quotaSize};
    map.quotaSize = StorageMapNoQuota;
    String oldValue;
    bool quotaException = false;
    _ZN7WebCore10StorageMap7setItemERKN3WTF6StringES4_RS2_Rb(&map, key, value, &oldValue, &quotaException);
}

void APS5_VABI _ZN7WebCore10StorageMap10removeItemERKN3WTF6StringERS2_(StorageMap* self, const String* key, String* oldValue) {
    StorageMap& map = Checked(self, __func__);
    CheckedString(key, __func__);
    if (!oldValue) throw std::invalid_argument(std::string(__func__) + ": null output reference");
    *oldValue = String();
    CheckedSize newSize{map.impl->currentSize, false};
    StringPair* entry = StringHashMapFind(map.impl->map, key->impl);
    if (!entry) return;
    *oldValue = Share(entry->value);
    newSize.Subtract(SizeInBytes(entry->key));
    newSize.Subtract(SizeInBytes(oldValue->impl));
    if (newSize.overflowed) throw std::overflow_error(std::string(__func__) + ": the size of the map underflows");
    if (map.impl->refCount == 1U)
        StringHashMapRemove(map.impl->map, entry);
    else {
        ReplaceWithCopy(map);
        StringHashMapRemove(map.impl->map, StringHashMapFind(map.impl->map, key->impl));
    }
    map.impl->currentSize = newSize.value;
    InvalidateIterator(*map.impl);
}

bool APS5_VABI _ZNK7WebCore10StorageMap8containsERKN3WTF6StringE(const StorageMap* self, const String* key) {
    const StorageMapImpl& impl = *Checked(self, __func__).impl;
    return StringHashMapFind(impl.map, CheckedString(key, __func__).impl) != nullptr;
}

}
