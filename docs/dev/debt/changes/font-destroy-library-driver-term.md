# libSceFont

## add Unknown function info

- [Unreadable library driver block](../../../core/libs/prx/libSceFont/src/Library.cpp): `sceFontDestroyLibrary` reads the driver's `term` callback with `GuestCopyIn_nid_no_patch`. When the driver block passed to library creation is no longer readable, it returns `SCE_FONT_ERROR_INVALID_PARAMETER` before anything is released, keeps the handle and restores `errno`. The console result for an unreadable driver block is not measured; this is a host-safety fallback.
