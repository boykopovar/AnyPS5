# libSceFreeTypeOt

## add Functional

- [FT_New_GlyphSlot, ft_glyphslot_free_bitmap, ft_glyphslot_set_bitmap and ft_glyphslot_alloc_bitmap guest pointers](../../../core/libs/prx/libSceFreeTypeOt/src/GlyphSlot.cpp) (libSceFreeTypeOt) - `face`, `driver`, `clazz`, `slot`, `face->glyph`, `aslot`, and `slot->bitmap.buffer` are read and written through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving caller `errno`, rolling back partial slot allocations and returning `Invalid_Argument` (`0x06`) or no-op on unreadable or unwritable pointers as host-safety fallbacks. Console behaviour on unmapped memory is unmeasured.
