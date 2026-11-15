# libSceSystemService

## add Functional

- [sceLncUtilIsAppLaunched and sceLncUtilIsSpeculativeLaunch guest pointers](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - returning `0x80940005` (`LNC_UTIL_ERROR_INVALID_ARGUMENT`) when `titleId` cannot be copied via `CopyGuestString_nid_no_patch` or when the boolean output pointer cannot be written via `GuestCopyOut_nid_no_patch` (preserving `errno`) is a host-safety fallback; console behavior on unmapped pointers is not measured.
