// Derived from Source/WTF/wtf/HashTable.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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
#include "prx/libSceNKWebKit/src/WtfHashMapStringAbi.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace NKWebKit::WtfAbi {

namespace {

constexpr unsigned HashFlagCount = 8;

StringImpl* DeletedKey() {
    return reinterpret_cast<StringImpl*>(~std::uintptr_t(0));
}

unsigned* Metadata(StringPair* table) {
    return reinterpret_cast<unsigned*>(table);
}

StringImpl* RetainImpl(StringImpl* impl) {
    if (impl) impl->refCount += 2U;
    return impl;
}

bool ShouldExpand(std::uint64_t keyAndDeletedCount, std::uint64_t tableSize) {
    if (tableSize <= StringHashMapMaxSmallTableCapacity)
        return keyAndDeletedCount * StringHashMapSmallMaxLoadDenominator >= tableSize * StringHashMapSmallMaxLoadNumerator;
    return keyAndDeletedCount * StringHashMapLargeMaxLoadDenominator >= tableSize * StringHashMapLargeMaxLoadNumerator;
}

unsigned RoundUpToPowerOfTwo(unsigned value) {
    value--;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value++;
    return value;
}

StringPair* AllocateTable(unsigned tableSize) {
    auto* storage = static_cast<unsigned char*>(
        _ZN3WTF16fastZeroedMallocEm(StringHashMapMetadataSize + std::size_t(tableSize) * sizeof(StringPair)));
    auto* table = reinterpret_cast<StringPair*>(storage + StringHashMapMetadataSize);
    Metadata(table)[StringHashMapTableSizeOffset] = tableSize;
    Metadata(table)[StringHashMapTableSizeMaskOffset] = tableSize - 1U;
    Metadata(table)[StringHashMapKeyCountOffset] = 0U;
    Metadata(table)[StringHashMapDeletedCountOffset] = 0U;
    return table;
}

void FreeTable(StringPair* table) {
    _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(table) - StringHashMapMetadataSize);
}

StringPair* LookupForReinsert(StringPair* table, const StringImpl* key) {
    unsigned sizeMask = Metadata(table)[StringHashMapTableSizeMaskOffset];
    unsigned i = StringHashMapKeyHash(key) & sizeMask;
    unsigned probeCount = 0U;
    while (true) {
        StringPair* entry = table + i;
        if (StringHashMapIsEmptyBucket(*entry)) return entry;
        ++probeCount;
        i = (i + probeCount) & sizeMask;
    }
}

StringPair* Rehash(StringHashMap& map, unsigned newTableSize, StringPair* entry) {
    StringPair* oldTable = map.table;
    unsigned oldTableSize = oldTable ? Metadata(oldTable)[StringHashMapTableSizeOffset] : 0U;
    unsigned oldKeyCount = oldTable ? Metadata(oldTable)[StringHashMapKeyCountOffset] : 0U;
    StringPair* table = AllocateTable(newTableSize);
    Metadata(table)[StringHashMapKeyCountOffset] = oldKeyCount;
    map.table = table;
    StringPair* newEntry = nullptr;
    for (unsigned i = 0U; i != oldTableSize; ++i) {
        StringPair& oldEntry = oldTable[i];
        if (StringHashMapIsDeletedBucket(oldEntry) || StringHashMapIsEmptyBucket(oldEntry)) continue;
        StringPair* reinsertedEntry = LookupForReinsert(table, oldEntry.key);
        reinsertedEntry->key = std::exchange(oldEntry.key, nullptr);
        reinsertedEntry->value = std::exchange(oldEntry.value, nullptr);
        if (&oldEntry == entry) newEntry = reinsertedEntry;
    }
    if (oldTable) FreeTable(oldTable);
    return newEntry;
}

StringPair* Expand(StringHashMap& map, StringPair* entry) {
    unsigned oldSize = StringHashMapTableSize(map);
    unsigned newSize;
    if (!oldSize)
        newSize = StringHashMapMinimumTableSize;
    else if (StringHashMapKeyCount(map) * StringHashMapMinLoad < oldSize * 2U)
        newSize = oldSize;
    else {
        if (oldSize > 0x40000000U) throw std::length_error("WTF::HashMap<String, String>: table cannot grow further");
        newSize = oldSize * 2U;
    }
    return Rehash(map, newSize, entry);
}

void SkipEmptyBuckets(StringHashMapIterator& iterator) {
    while (iterator.position != iterator.endPosition &&
           (StringHashMapIsEmptyBucket(*iterator.position) || StringHashMapIsDeletedBucket(*iterator.position)))
        ++iterator.position;
}

}  // namespace

unsigned StringHashMapTableSize(const StringHashMap& map) {
    return map.table ? Metadata(map.table)[StringHashMapTableSizeOffset] : 0U;
}

unsigned StringHashMapKeyCount(const StringHashMap& map) {
    return map.table ? Metadata(map.table)[StringHashMapKeyCountOffset] : 0U;
}

unsigned StringHashMapDeletedCount(const StringHashMap& map) {
    return map.table ? Metadata(map.table)[StringHashMapDeletedCountOffset] : 0U;
}

bool StringHashMapIsEmptyBucket(const StringPair& bucket) {
    return !bucket.key;
}

bool StringHashMapIsDeletedBucket(const StringPair& bucket) {
    return bucket.key == DeletedKey();
}

unsigned StringHashMapKeyHash(const StringImpl* key) {
    if (!key) throw std::invalid_argument("WTF::HashMap<String, String>: null key");
    unsigned hash = key->hashAndFlags >> HashFlagCount;
    return hash ? hash : _ZNK3WTF10StringImpl12hashSlowCaseEv(key);
}

unsigned StringHashMapBestTableSize(unsigned keyCount) {
    unsigned bestTableSize = RoundUpToPowerOfTwo(keyCount);
    if (ShouldExpand(keyCount, bestTableSize)) bestTableSize *= 2U;
    auto aboveThresholdForEagerExpansion = [](double loadFactor, unsigned count, unsigned tableSize) {
        double maxLoadRatio = loadFactor;
        double minLoadRatio = 1.0 / StringHashMapMinLoad;
        double averageLoadRatio = (maxLoadRatio + minLoadRatio) / 2;
        double halfWayBetweenAverageAndMaxLoadRatio = (averageLoadRatio + maxLoadRatio) / 2;
        return count >= tableSize * halfWayBetweenAverageAndMaxLoadRatio;
    };
    if (bestTableSize <= StringHashMapMaxSmallTableCapacity) {
        constexpr double smallLoadFactor =
            static_cast<double>(StringHashMapSmallMaxLoadNumerator) / StringHashMapSmallMaxLoadDenominator;
        if (aboveThresholdForEagerExpansion(smallLoadFactor, keyCount, bestTableSize)) bestTableSize *= 2U;
    } else {
        constexpr double largeLoadFactor =
            static_cast<double>(StringHashMapLargeMaxLoadNumerator) / StringHashMapLargeMaxLoadDenominator;
        if (aboveThresholdForEagerExpansion(largeLoadFactor, keyCount, bestTableSize)) bestTableSize *= 2U;
    }
    return std::max(bestTableSize, StringHashMapMinimumTableSize);
}

StringPair* StringHashMapFind(const StringHashMap& map, const StringImpl* key) {
    StringPair* table = map.table;
    if (!table) return nullptr;
    unsigned sizeMask = Metadata(table)[StringHashMapTableSizeMaskOffset];
    unsigned i = StringHashMapKeyHash(key) & sizeMask;
    unsigned probeCount = 0U;
    while (true) {
        StringPair* entry = table + i;
        if (StringHashMapIsEmptyBucket(*entry)) return nullptr;
        if (!StringHashMapIsDeletedBucket(*entry) && _ZN3WTF5equalEPKNS_10StringImplES2_(entry->key, key)) return entry;
        ++probeCount;
        i = (i + probeCount) & sizeMask;
    }
}

StringHashMapAddResult StringHashMapAdd(StringHashMap& map, StringImpl* key, StringImpl* value) {
    unsigned hash = StringHashMapKeyHash(key);
    if (!map.table) Expand(map, nullptr);
    StringPair* table = map.table;
    unsigned sizeMask = Metadata(table)[StringHashMapTableSizeMaskOffset];
    unsigned i = hash & sizeMask;
    unsigned probeCount = 0U;
    StringPair* deletedEntry = nullptr;
    StringPair* entry;
    while (true) {
        entry = table + i;
        if (StringHashMapIsEmptyBucket(*entry)) break;
        if (StringHashMapIsDeletedBucket(*entry))
            deletedEntry = entry;
        else if (_ZN3WTF5equalEPKNS_10StringImplES2_(entry->key, key))
            return {entry, false};
        ++probeCount;
        i = (i + probeCount) & sizeMask;
    }
    if (deletedEntry) {
        deletedEntry->key = nullptr;
        deletedEntry->value = nullptr;
        entry = deletedEntry;
        Metadata(table)[StringHashMapDeletedCountOffset] -= 1U;
    }
    entry->key = RetainImpl(key);
    entry->value = RetainImpl(value);
    Metadata(table)[StringHashMapKeyCountOffset] += 1U;
    if (ShouldExpand(StringHashMapKeyCount(map) + StringHashMapDeletedCount(map), StringHashMapTableSize(map)))
        entry = Expand(map, entry);
    return {entry, true};
}

StringHashMapAddResult StringHashMapSet(StringHashMap& map, StringImpl* key, StringImpl* value) {
    StringHashMapAddResult result = StringHashMapAdd(map, key, value);
    if (!result.isNewEntry) Release(std::exchange(result.entry->value, RetainImpl(value)));
    return result;
}

void StringHashMapRemove(StringHashMap& map, StringPair* entry) {
    if (!entry) return;
    StringPair* table = map.table;
    if (!table) throw std::invalid_argument("WTF::HashMap<String, String>: entry of an empty table");
    unsigned tableSize = Metadata(table)[StringHashMapTableSizeOffset];
    const auto address = reinterpret_cast<std::uintptr_t>(entry);
    const auto first = reinterpret_cast<std::uintptr_t>(table);
    const auto last = first + std::uintptr_t(tableSize) * sizeof(StringPair);
    if (address < first || address >= last || (address - first) % sizeof(StringPair) != 0 ||
        StringHashMapIsEmptyBucket(*entry) || StringHashMapIsDeletedBucket(*entry))
        throw std::invalid_argument("WTF::HashMap<String, String>: not an entry of this table");
    Release(std::exchange(entry->key, DeletedKey()));
    Release(std::exchange(entry->value, nullptr));
    unsigned* metadata = Metadata(table);
    metadata[StringHashMapDeletedCountOffset] += 1U;
    metadata[StringHashMapKeyCountOffset] -= 1U;
    if (metadata[StringHashMapKeyCountOffset] * StringHashMapMinLoad < tableSize && tableSize > StringHashMapMinimumTableSize)
        Rehash(map, tableSize / 2U, nullptr);
}

StringHashMapIterator StringHashMapEnd(const StringHashMap& map) {
    StringPair* end = map.table ? map.table + StringHashMapTableSize(map) : nullptr;
    return {end, end};
}

StringHashMapIterator StringHashMapBegin(const StringHashMap& map) {
    if (!StringHashMapKeyCount(map)) return StringHashMapEnd(map);
    StringHashMapIterator iterator{map.table, map.table + StringHashMapTableSize(map)};
    SkipEmptyBuckets(iterator);
    return iterator;
}

void StringHashMapAdvance(StringHashMapIterator& iterator) {
    if (iterator.position == iterator.endPosition)
        throw std::out_of_range("WTF::HashMap<String, String>: iterator advanced past the end");
    ++iterator.position;
    SkipEmptyBuckets(iterator);
}

void StringHashMapAssign(StringHashMap& destination, const StringHashMap& source) {
    StringHashMap copy{nullptr};
    unsigned otherKeyCount = StringHashMapKeyCount(source);
    if (otherKeyCount) {
        copy.table = AllocateTable(StringHashMapBestTableSize(otherKeyCount));
        Metadata(copy.table)[StringHashMapKeyCountOffset] = otherKeyCount;
        for (StringHashMapIterator iterator = StringHashMapBegin(source); iterator.position != iterator.endPosition;
             StringHashMapAdvance(iterator)) {
            StringPair* entry = LookupForReinsert(copy.table, iterator.position->key);
            entry->key = RetainImpl(iterator.position->key);
            entry->value = RetainImpl(iterator.position->value);
        }
    }
    std::swap(destination.table, copy.table);
    StringHashMapClear(copy);
}

void StringHashMapClear(StringHashMap& map) {
    StringPair* table = std::exchange(map.table, nullptr);
    if (!table) return;
    unsigned tableSize = Metadata(table)[StringHashMapTableSizeOffset];
    for (unsigned i = 0U; i < tableSize; ++i) {
        StringPair& bucket = table[i];
        if (StringHashMapIsDeletedBucket(bucket)) continue;
        Release(std::exchange(bucket.key, nullptr));
        Release(std::exchange(bucket.value, nullptr));
    }
    FreeTable(table);
}

}  // namespace NKWebKit::WtfAbi
