#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WtfAbi {

struct StringPair {
    StringImpl* key;
    StringImpl* value;
};

struct StringHashMap {
    StringPair* table;
};

struct StringHashMapIterator {
    StringPair* position;
    StringPair* endPosition;
};

struct StringHashMapAddResult {
    StringPair* entry;
    bool isNewEntry;
};

inline constexpr std::size_t StringHashMapMetadataSize = 4 * sizeof(unsigned);
inline constexpr unsigned StringHashMapMinimumTableSize = 8;
inline constexpr unsigned StringHashMapMinLoad = 6;
inline constexpr unsigned StringHashMapSmallMaxLoadNumerator = 3;
inline constexpr unsigned StringHashMapSmallMaxLoadDenominator = 4;
inline constexpr unsigned StringHashMapLargeMaxLoadNumerator = 1;
inline constexpr unsigned StringHashMapLargeMaxLoadDenominator = 2;
inline constexpr unsigned StringHashMapMaxSmallTableCapacity = 1024;
inline constexpr int StringHashMapTableSizeOffset = -1;
inline constexpr int StringHashMapTableSizeMaskOffset = -2;
inline constexpr int StringHashMapKeyCountOffset = -3;
inline constexpr int StringHashMapDeletedCountOffset = -4;

unsigned StringHashMapTableSize(const StringHashMap& map);
unsigned StringHashMapKeyCount(const StringHashMap& map);
unsigned StringHashMapDeletedCount(const StringHashMap& map);
bool StringHashMapIsEmptyBucket(const StringPair& bucket);
bool StringHashMapIsDeletedBucket(const StringPair& bucket);
unsigned StringHashMapKeyHash(const StringImpl* key);
unsigned StringHashMapBestTableSize(unsigned keyCount);
StringPair* StringHashMapFind(const StringHashMap& map, const StringImpl* key);
StringHashMapAddResult StringHashMapAdd(StringHashMap& map, StringImpl* key, StringImpl* value);
StringHashMapAddResult StringHashMapSet(StringHashMap& map, StringImpl* key, StringImpl* value);
void StringHashMapRemove(StringHashMap& map, StringPair* entry);
StringHashMapIterator StringHashMapBegin(const StringHashMap& map);
StringHashMapIterator StringHashMapEnd(const StringHashMap& map);
void StringHashMapAdvance(StringHashMapIterator& iterator);
void StringHashMapAssign(StringHashMap& destination, const StringHashMap& source);
void StringHashMapClear(StringHashMap& map);

static_assert(sizeof(StringPair) == 16);
static_assert(alignof(StringPair) == 8);
static_assert(offsetof(StringPair, key) == 0);
static_assert(offsetof(StringPair, value) == 8);
static_assert(std::is_trivially_copyable_v<StringPair>);
static_assert(sizeof(StringHashMap) == 8);
static_assert(alignof(StringHashMap) == 8);
static_assert(offsetof(StringHashMap, table) == 0);
static_assert(std::is_trivially_copyable_v<StringHashMap>);
static_assert(sizeof(StringHashMapIterator) == 16);
static_assert(alignof(StringHashMapIterator) == 8);
static_assert(offsetof(StringHashMapIterator, position) == 0);
static_assert(offsetof(StringHashMapIterator, endPosition) == 8);
static_assert(std::is_trivially_copyable_v<StringHashMapIterator>);
static_assert(StringHashMapMetadataSize == 16);
static_assert(StringHashMapMetadataSize % alignof(StringPair) == 0);

}  // namespace NKWebKit::WtfAbi
