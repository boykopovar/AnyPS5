# libSceFreeTypeOl

## add Functional

- [sceFreeTypeOlFree, sceFreeTypeOlFtaSupportModules and sceFreeTypeOlDoneGlyphSlot guest pointers](../../../core/libs/prx/libSceFreeTypeOl/ModuleWrappers.cpp) (libSceFreeTypeOl) - `memory`, `block`, `library`, `slot`, and `owner->host` are read and written through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving caller `errno`, returning `0x21` or no-op on unreadable or unwritable pointers as host-safety fallbacks instead of calling `guestMemory.free`. Console behaviour on unmapped memory is unmeasured.
