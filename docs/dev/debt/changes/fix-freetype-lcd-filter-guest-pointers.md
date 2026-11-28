# libSceFreeType

## add Functional

- [FT_Library_SetLcdFilter and FT_Library_SetLcdFilterWeights](../../../core/libs/prx/libSceFreeType/src/LcdFilter.cpp) copy guest `library->host` and `weights` bytes through `GuestCopyIn_nid_no_patch`, preserving `errno` and failing through `NotImplemented_nid_no_patch` on unreadable guest addresses.
