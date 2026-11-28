# libSceFreeTypeOt

## add Functional

- [sceFreeTypeOtGlyphSlotOwnBitmap](../../../core/libs/prx/libSceFreeTypeOt/src/Bitmap.cpp) reads `slot`, `slot->internal`, `slot->library`, and bitmap bytes through `GuestCopyIn_nid_no_patch` and updates `slot->bitmap` and `internal->flags` through `GuestCopyOut_nid_no_patch` while preserving `errno`, rolling back allocated buffers and slot state and rejecting unreadable or unwritable guest pointers via `NotImplemented_nid_no_patch`.
