# libSceSystemService

## add Unknown function info

- [LoadExec paths](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - `sceSystemServiceLoadExec` and `sceLncUtilLoadExec` copy the path in with `CopyGuestPath_nid_no_patch` (1024 bytes, like the other path calls) before checking it; an unreadable or over-long path returns `SCE_SYSTEM_SERVICE_ERROR_PARAMETER` / `SCE_LNC_UTIL_ERROR_INVALID_ARGUMENT` like a null one, because the console's behaviour on unmapped memory is unmeasured. The argument vector is still read in place.
