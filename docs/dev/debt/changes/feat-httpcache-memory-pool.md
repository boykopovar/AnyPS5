# libSceHttpCache

## change `RemoveTag`, `ClearCacheInfo` and both `WithMemoryPool` forms

- [libSceHttpCache](../../../core/libs/prx/libSceHttpCache/Export.cpp) - `SetQuota`, a non-null `RegisterCacheInfoUrl` and `RemoveTag` throw after validation; `ClearCacheInfo` and `RemoveTag` signatures remain unverified. `RevalidateWithMemoryPool` follows the complete 13.42 wrapper reading in [issue #21850](https://github.com/boykopovar/AnyPS5/issues/21850#issuecomment-6962993274). `RetrieveWithMemoryPool` size queries and pool filling follow static disassembly of the supplied PRX (SHA-256 `fa6d9e547b08604e4fb7c0245f68800ea6fafaadf7bc5a1328d198744600a7fb`); its firmware version is unknown and it was not measured on a console. It writes next-request headers and URL, then adds a usable stale response. Tagged entries are cleared by user; `CreateRequestWithTag` rejects null, empty or over-31-byte tags. Tag rules are not measured. `Init` ignores pool size and stores do not reproduce file errors.

## add Functional

- [sceHttpCacheRetrieveWithMemoryPool and sceHttpCacheRevalidateWithMemoryPool](../../../core/libs/prx/libSceHttpCache/Export.cpp) (libSceHttpCache) - pool size queries and copies follow the documented 13.42 revalidation reading and static disassembly of a supplied PRX whose firmware version is unidentified. Retrieve pool output writes the next request first and appends a usable stale response; a null request still throws. Revalidation with a null header and non-null `result` reaches its core as empty input. Invalid guest pointers return `0x817a0012` as a host-safety fallback. The supplied image was not measured on a console.
