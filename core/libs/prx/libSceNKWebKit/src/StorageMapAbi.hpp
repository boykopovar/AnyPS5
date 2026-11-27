#pragma once

#include "prx/libSceNKWebKit/src/WtfHashMapStringAbi.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct StorageMapImpl {
    unsigned refCount;
    WtfAbi::StringHashMap map;
    WtfAbi::StringHashMapIterator iterator;
    unsigned iteratorIndex;
    unsigned currentSize;
};

struct StorageMap {
    StorageMapImpl* impl;
    unsigned quotaSize;
};

inline constexpr unsigned StorageMapNoQuota = 0xFFFFFFFFU;

StorageMapImpl* CreateStorageMapImpl();
void DerefStorageMapImpl(StorageMapImpl* impl);

static_assert(sizeof(StorageMapImpl) == 40);
static_assert(alignof(StorageMapImpl) == 8);
static_assert(offsetof(StorageMapImpl, refCount) == 0);
static_assert(offsetof(StorageMapImpl, map) == 8);
static_assert(offsetof(StorageMapImpl, iterator) == 16);
static_assert(offsetof(StorageMapImpl, iteratorIndex) == 32);
static_assert(offsetof(StorageMapImpl, currentSize) == 36);
static_assert(std::is_trivially_copyable_v<StorageMapImpl>);
static_assert(sizeof(StorageMap) == 16);
static_assert(alignof(StorageMap) == 8);
static_assert(offsetof(StorageMap, impl) == 0);
static_assert(offsetof(StorageMap, quotaSize) == 8);
static_assert(std::is_trivially_copyable_v<StorageMap>);

}  // namespace NKWebKit::WebCoreAbi
