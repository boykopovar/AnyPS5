# libSceNKWebKit

## add Functional

- [StorageMap guest pointer validation](../../../core/libs/prx/libSceNKWebKit/src/StorageMap.cpp) - console behaviour on inaccessible or read-only `StorageMap`, `StorageMapImpl`, `WTF::String` slot/header/character buffers, or `quotaException` output pointers in `WebCore::StorageMap::key`, `getItem`, `setItem`, `setItemIgnoringQuota`, `removeItem`, and `contains` is unmeasured; internal `StorageMapImpl::map.table` buckets, cached iterators, and already-stored map entries are assumed valid once owned by the map. Closes when: console behaviour on invalid guest pointers in these `WebCore::StorageMap` exports is measured.
