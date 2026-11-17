# libSceHttpCache

## change [libSceHttpCache additional exports]

- [libSceHttpCache additional exports](../../../core/libs/prx/libSceHttpCache/Export.cpp) - this name/NID pair is present in the inspected export inventory. Firmware 13.42 gives `sceHttpCacheEnableCacheInfo` one `bool` ([reading](https://github.com/boykopovar/AnyPS5/issues/21850#issuecomment-6939803983), disassembly only); it still throws through `NotImplemented_nid_no_patch`, because its result comes from `0x2e260`, which was not read, and the call talks to a system service over IPMI ([follow-up](https://github.com/boykopovar/AnyPS5/issues/21850#issuecomment-6958852739)). No runtime functionality is implemented.

## remove | `sceHttpCacheUnregisterCacheInfoUrl` | `3JQc1T-fvmc` |

## change [`sceHttpCacheClearCacheInfo` and `sceHttpCacheRemoveTag`]

- [`sceHttpCacheRemoveTag`](../../../core/libs/prx/libSceHttpCache/Export.cpp): the type of its last parameter is unknown, so it is declared as an untyped pointer; firmware 13.42 reads a string pointer at its offset 8, so it may be a request record ([reading](https://github.com/boykopovar/AnyPS5/issues/21850#issuecomment-6939803983)). The pointer types of [`sceHttpCacheClearAllUserTags`](../../../core/libs/prx/libSceHttpCache/Export.cpp) (a list of at most 20 entries, taken as user ids) and [`sceHttpCacheRegisterCacheInfoUrl`](../../../core/libs/prx/libSceHttpCache/Export.cpp) are inferred from the checks.

## add Functional

- [sceHttpCacheClearCacheInfo, sceHttpCacheUnregisterCacheInfoUrl and sceHttpCacheRemoveTag](../../../core/libs/prx/libSceHttpCache/Export.cpp) (libSceHttpCache) - `ClearCacheInfo` and a non-null `UnregisterCacheInfoUrl` return `0x817a0014` (`ErrorCacheInfoDisabled`, a name chosen here), firmware 13.42's answer when no cache-info object exists. That holds only while `EnableCacheInfo`, which creates the object and talks to an IPMI system service ([follow-up](https://github.com/boykopovar/AnyPS5/issues/21850#issuecomment-6958852739)), throws; implementing it means modelling the object here. `RemoveTag` throws through `NotImplemented_nid_no_patch` past its checks: its worker (`0x21540`) was not read. The checks follow the [reading](https://github.com/boykopovar/AnyPS5/issues/21850#issuecomment-6939803983) (disassembly only): a null URL returns `0x817a0012`, and an unknown cache or a null tag or last argument `0x817a0011`. Closes when: `0x21540` and `EnableCacheInfo`'s service are read.
