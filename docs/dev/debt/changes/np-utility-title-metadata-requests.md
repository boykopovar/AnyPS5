# libSceNpUtility

## remove [sceNpTitleMetadataIntAbortRequest, sceNpTitleMetadataIntCreateRequest and sceNpTitleMetadataIntDeleteRequest]

## add Functional

- [sceNpTitleMetadataIntCreateRequest, sceNpTitleMetadataIntAbortRequest and sceNpTitleMetadataIntDeleteRequest](../../../core/libs/prx/libSceNpUtility/Export.cpp) (libSceNpUtility) - request lifecycle from the 11.00 module (`0x199f0`, `0x19cb0`, `0x19d40`): CreateRequest lazily initializes and assigns sequential 24-bit IDs (`1..0xFFFFFF`, wrapping to `1` and skipping live IDs, with a host safety cap of `0x1000` returning `0x8055AA05`), AbortRequest returns `0x8055AA09` when uninitialized/missing and `0` when live, and DeleteRequest returns `0x8055AA09` when uninitialized/missing or erases the ID and returns `0` (clearing active state when the last live request is deleted). HTTP requests, payload allocations and in-flight state codes are not modelled. Not measured on a PS5.
