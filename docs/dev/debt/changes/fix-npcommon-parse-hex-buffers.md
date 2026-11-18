# libSceNpCommon

## add Unknown function info

- [sceNpStrParseHex and sceNpStrnParseHex buffers](../../../core/libs/prx/libSceNpCommon/Strings.cpp) (libSceNpCommon) - the hex text is copied in with `GuestCopyIn_nid_no_patch` in 4 KiB-aligned chunks (up to its terminator for `sceNpStrParseHex`), so a large counted length on unreadable text fails before it is allocated, and the bytes parsed before a refused digit are written with one `GuestCopyOut_nid_no_patch`; an unreadable text or unwritable output returns `SCE_NP_ERROR_INVALID_ARGUMENT` with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured. `sceNpStrnParseHex` reads the whole counted text before parsing, so an unreadable byte after a refused digit is refused instead of storing the earlier bytes.
