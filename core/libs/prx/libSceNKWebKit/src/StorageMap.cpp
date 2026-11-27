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

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/StorageMapExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
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

static_assert(sizeof(String) == sizeof(StringImpl*));

constexpr std::size_t GuestProbeChunkBytes = 4096;

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

template <typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    T value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template <typename T>
void RequireWritableGuestValue(T* pointer, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestWritable_nid_no_patch(pointer, sizeof(T));
    errno = savedErrno;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template <typename T>
bool TryWriteGuestValue(T* pointer, const T& value) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(pointer, &value, sizeof(T));
    errno = savedErrno;
    return ok;
}

template <typename T>
void WriteGuestValue(T* pointer, const T& value, const char* function) {
    if (!TryWriteGuestValue(pointer, value)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireReadableGuestBytes(const void* pointer, std::size_t bytes, const char* function) {
    if (bytes == 0) return;
    constexpr auto maxAddress = std::numeric_limits<std::uintptr_t>::max();
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    if (address == 0 || bytes > maxAddress - address) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    std::array<std::byte, GuestProbeChunkBytes> scratch{};
    const auto* cursor = static_cast<const std::byte*>(pointer);
    std::size_t remaining = bytes;
    while (remaining > 0) {
        const std::size_t step = std::min(remaining, scratch.size());
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(scratch.data(), cursor, step);
        errno = savedErrno;
        if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        cursor += step;
        remaining -= step;
    }
}

StorageMap LoadStorageMap(const StorageMap* self, const char* function) {
    if (!self) throw std::invalid_argument(std::string(function) + ": null StorageMap");
    const auto map = ReadGuestValue(self, function);
    if (!map.impl) throw std::invalid_argument(std::string(function) + ": null StorageMap");
    return map;
}

void LoadReadableStorageMapImpl(const StorageMapImpl* impl, const char* function) {
    static_cast<void>(ReadGuestValue(impl, function));
}

void RequireWritableStorageMapImpl(StorageMapImpl* impl, const char* function) {
    static_cast<void>(ReadGuestValue(impl, function));
    RequireWritableGuestValue(impl, function);
}

StringImpl* ReadGuestStringSlot(const String* string, const char* function) {
    if (!string) throw std::invalid_argument(std::string(function) + ": null String reference");
    return ReadGuestValue(reinterpret_cast<StringImpl* const*>(string), function);
}

void RequireWritableStringSlot(String* string, const char* function) {
    if (!string) throw std::invalid_argument(std::string(function) + ": null output reference");
    StringImpl* previous = ReadGuestValue(reinterpret_cast<StringImpl* const*>(string), function);
    RequireWritableGuestValue(reinterpret_cast<StringImpl**>(string), function);
    if (previous) {
        static_cast<void>(ReadGuestValue(previous, function));
        RequireWritableGuestValue(previous, function);
    }
}

bool TryPublishGuestStringSlot(String* target, String& value) {
    StringImpl* previous = nullptr;
    const int savedErrno = errno;
    const bool readOk = GuestCopyIn_nid_no_patch(&previous, reinterpret_cast<StringImpl* const*>(target), sizeof(previous));
    errno = savedErrno;
    if (!readOk) return false;
    StringImpl* next = value.impl;
    if (!TryWriteGuestValue(reinterpret_cast<StringImpl**>(target), next)) {
        static_cast<void>(TryWriteGuestValue(reinterpret_cast<StringImpl**>(target), previous));
        return false;
    }
    value.impl = nullptr;
    NKWebKit::WtfAbi::Release(previous);
    return true;
}

void PublishGuestStringSlot(String* target, String value, const char* function) {
    if (!TryPublishGuestStringSlot(target, value)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void ValidateGuestStringImpl(StringImpl* impl, bool writableHeader, const char* function) {
    if (!impl) return;
    const auto header = ReadGuestValue(impl, function);
    if (header.length > 0) {
        const bool is8Bit = (header.hashAndFlags & 4U) != 0U;
        const std::size_t bytes = static_cast<std::size_t>(header.length) * (is8Bit ? 1U : 2U);
        RequireReadableGuestBytes(header.data, bytes, function);
    }
    if (writableHeader || (header.hashAndFlags >> 8U) == 0U) RequireWritableGuestValue(impl, function);
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

StorageMapImpl* CloneStorageMapImpl(const StorageMapImpl& source) {
    StorageMapImpl* copy = NKWebKit::WebCoreAbi::CreateStorageMapImpl();
    try {
        StringHashMapAssign(copy->map, source.map);
    } catch (...) {
        NKWebKit::WebCoreAbi::DerefStorageMapImpl(copy);
        throw;
    }
    copy->currentSize = source.currentSize;
    return copy;
}

void SetItemImpl(StorageMap* self, const String* key, const String* value, String* oldValue, bool* quotaException,
                 bool ignoreQuota, const char* function) {
    StorageMap localMap = LoadStorageMap(self, function);
    StringImpl* keyImpl = ReadGuestStringSlot(key, function);
    StringImpl* valueImpl = ReadGuestStringSlot(value, function);
    if (!oldValue || !quotaException) throw std::invalid_argument(std::string(function) + ": null output reference");
    RequireWritableStringSlot(oldValue, function);
    const bool previousQuota = ReadGuestValue(quotaException, function);
    RequireWritableGuestValue(quotaException, function);
    LoadReadableStorageMapImpl(localMap.impl, function);
    ValidateGuestStringImpl(keyImpl, key == oldValue, function);
    ValidateGuestStringImpl(valueImpl, value == oldValue, function);

    CheckedSize newSize{localMap.impl->currentSize, false};
    const StringPair* entry = StringHashMapFind(localMap.impl->map, keyImpl);
    String nextOldValue;
    if (entry) {
        nextOldValue = Share(entry->value);
        newSize.Subtract(SizeInBytes(entry->value));
    } else {
        newSize.Add(SizeInBytes(keyImpl));
    }
    newSize.Add(SizeInBytes(valueImpl));
    if (!ignoreQuota && localMap.quotaSize != StorageMapNoQuota &&
        (newSize.overflowed || newSize.value > localMap.quotaSize)) {
        if (!TryWriteGuestValue(quotaException, true)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        if (!TryPublishGuestStringSlot(oldValue, nextOldValue)) {
            static_cast<void>(TryWriteGuestValue(quotaException, previousQuota));
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
        return;
    }
    if (newSize.overflowed) throw std::overflow_error(std::string(function) + ": the size of the map overflows 32 bits");
    if (!keyImpl) throw std::invalid_argument(std::string(function) + ": null key");

    const bool copyOnWrite = localMap.impl->refCount > 1U;
    if (copyOnWrite) RequireWritableGuestValue(self, function);
    RequireWritableStorageMapImpl(localMap.impl, function);
    if (!entry) ValidateGuestStringImpl(keyImpl, true, function);
    if (valueImpl) ValidateGuestStringImpl(valueImpl, true, function);

    if (copyOnWrite) {
        StorageMapImpl* copy = CloneStorageMapImpl(*localMap.impl);
        try {
            StringHashMapSet(copy->map, keyImpl, valueImpl);
            copy->currentSize = newSize.value;
            InvalidateIterator(*copy);
        } catch (...) {
            NKWebKit::WebCoreAbi::DerefStorageMapImpl(copy);
            throw;
        }
        StorageMap updated{copy, localMap.quotaSize};
        if (!TryWriteGuestValue(self, updated)) {
            static_cast<void>(TryWriteGuestValue(self, localMap));
            NKWebKit::WebCoreAbi::DerefStorageMapImpl(copy);
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
        NKWebKit::WebCoreAbi::DerefStorageMapImpl(localMap.impl);
    } else {
        StringHashMapSet(localMap.impl->map, keyImpl, valueImpl);
        localMap.impl->currentSize = newSize.value;
        InvalidateIterator(*localMap.impl);
    }

    PublishGuestStringSlot(oldValue, std::move(nextOldValue), function);
    WriteGuestValue(quotaException, false, function);
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
    StorageMap localMap = LoadStorageMap(self, __func__);
    LoadReadableStorageMapImpl(localMap.impl, __func__);
    StorageMapImpl& impl = *localMap.impl;
    if (index >= StringHashMapKeyCount(impl.map)) return String();
    if (impl.iteratorIndex != index) RequireWritableStorageMapImpl(localMap.impl, __func__);
    SetIteratorToIndex(impl, index);
    if (impl.iterator.position == impl.iterator.endPosition)
        throw std::logic_error(std::string(__func__) + ": iterator past the last entry");
    return Share(impl.iterator.position->key);
}

String APS5_VABI _ZNK7WebCore10StorageMap7getItemERKN3WTF6StringE(const StorageMap* self, const String* key) {
    const StorageMap localMap = LoadStorageMap(self, __func__);
    LoadReadableStorageMapImpl(localMap.impl, __func__);
    StringImpl* keyImpl = ReadGuestStringSlot(key, __func__);
    ValidateGuestStringImpl(keyImpl, false, __func__);
    const StringPair* entry = StringHashMapFind(localMap.impl->map, keyImpl);
    if (!entry) return String();
    return Share(entry->value);
}

void APS5_VABI _ZN7WebCore10StorageMap7setItemERKN3WTF6StringES4_RS2_Rb(StorageMap* self, const String* key, const String* value,
                                                                          String* oldValue, bool* quotaException) {
    SetItemImpl(self, key, value, oldValue, quotaException, false, __func__);
}

void APS5_VABI _ZN7WebCore10StorageMap20setItemIgnoringQuotaERKN3WTF6StringES4_(StorageMap* self, const String* key, const String* value) {
    String oldValue;
    bool quotaException = false;
    SetItemImpl(self, key, value, &oldValue, &quotaException, true, __func__);
}

void APS5_VABI _ZN7WebCore10StorageMap10removeItemERKN3WTF6StringERS2_(StorageMap* self, const String* key, String* oldValue) {
    StorageMap localMap = LoadStorageMap(self, __func__);
    StringImpl* keyImpl = ReadGuestStringSlot(key, __func__);
    if (!oldValue) throw std::invalid_argument(std::string(__func__) + ": null output reference");
    RequireWritableStringSlot(oldValue, __func__);
    LoadReadableStorageMapImpl(localMap.impl, __func__);
    ValidateGuestStringImpl(keyImpl, key == oldValue, __func__);

    StringPair* entry = StringHashMapFind(localMap.impl->map, keyImpl);
    if (!entry) {
        PublishGuestStringSlot(oldValue, String(), __func__);
        return;
    }
    String nextOldValue = Share(entry->value);
    CheckedSize newSize{localMap.impl->currentSize, false};
    newSize.Subtract(SizeInBytes(entry->key));
    newSize.Subtract(SizeInBytes(entry->value));
    if (newSize.overflowed) throw std::overflow_error(std::string(__func__) + ": the size of the map underflows");

    const bool copyOnWrite = localMap.impl->refCount > 1U;
    if (copyOnWrite) RequireWritableGuestValue(self, __func__);
    RequireWritableStorageMapImpl(localMap.impl, __func__);

    if (!copyOnWrite) {
        StringHashMapRemove(localMap.impl->map, entry);
        localMap.impl->currentSize = newSize.value;
        InvalidateIterator(*localMap.impl);
    } else {
        StorageMapImpl* copy = CloneStorageMapImpl(*localMap.impl);
        try {
            StringHashMapRemove(copy->map, StringHashMapFind(copy->map, keyImpl));
            copy->currentSize = newSize.value;
            InvalidateIterator(*copy);
        } catch (...) {
            NKWebKit::WebCoreAbi::DerefStorageMapImpl(copy);
            throw;
        }
        StorageMap updated{copy, localMap.quotaSize};
        if (!TryWriteGuestValue(self, updated)) {
            static_cast<void>(TryWriteGuestValue(self, localMap));
            NKWebKit::WebCoreAbi::DerefStorageMapImpl(copy);
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        }
        NKWebKit::WebCoreAbi::DerefStorageMapImpl(localMap.impl);
    }
    PublishGuestStringSlot(oldValue, std::move(nextOldValue), __func__);
}

bool APS5_VABI _ZNK7WebCore10StorageMap8containsERKN3WTF6StringE(const StorageMap* self, const String* key) {
    const StorageMap localMap = LoadStorageMap(self, __func__);
    LoadReadableStorageMapImpl(localMap.impl, __func__);
    StringImpl* keyImpl = ReadGuestStringSlot(key, __func__);
    ValidateGuestStringImpl(keyImpl, false, __func__);
    return StringHashMapFind(localMap.impl->map, keyImpl) != nullptr;
}

}
