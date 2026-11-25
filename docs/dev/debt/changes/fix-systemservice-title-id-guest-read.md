# libSceSystemService

## add Functional

- [sceSystemServiceGetAppId and sceLncUtilIsAppLaunched](../../../core/libs/prx/libSceSystemService/Export.cpp) (libSceSystemService) - the title ID is read one byte at a time through `GuestCopyIn_nid_no_patch`, up to the 10-byte buffer and stopping at the terminator, with the caller's `errno` preserved; an unreadable pointer is answered as a malformed title ID (`-1` for `sceSystemServiceGetAppId`, `0x80940005` for `sceLncUtilIsAppLaunched`) instead of being read by `strlen` in the host. That answer is a host-safety fallback: the firmware reads the pointer unchecked and faults on it, which is not measured on a console. A title ID that is not terminated within those 10 bytes is rejected without reading past them.
