# libSceHttpCache

## change [sceHttpCacheReadData, sceHttpCacheRetrieve, sceHttpCacheRevalidate

- [sceHttpCacheReadData, sceHttpCacheRetrieve, sceHttpCacheRevalidate, sceHttpCacheSetData, sceHttpCacheSetQuota and sceHttpCacheSetResponseHeader](../../../core/libs/prx/libSceHttpCache/Export.cpp) (libSceHttpCache) - probing guest input and output pointers via `GuestCopyIn_nid_no_patch`, `GuestCopyOut_nid_no_patch`, and `CopyGuestString_nid_no_patch` (preserving `errno`), bounding `sceHttpCacheSetData` chunks at 64 MiB and `sceHttpCacheSetQuota` entries at 4096, and returning `0x817a0012` (`ErrorInvalidValue`) on unreadable, unwritable, or oversized arguments (with `sceHttpCacheRetrieve` and `sceHttpCacheRevalidate` rolling back state and store changes if copying outputs fails) is a host-safety fallback; console behaviour on unmapped memory or oversized counts is unmeasured.
