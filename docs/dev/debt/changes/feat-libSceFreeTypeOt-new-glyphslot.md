# libSceFreeTypeOt

## add Functional

- [FT_New_GlyphSlot](../../../core/libs/prx/libSceFreeTypeOt/src/GlyphSlot.cpp) (libSceFreeTypeOt) - a null face and a face without a driver both return Invalid_Argument (0x06), as the 2.4.12 [ftobjs.c](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftobjs.c#L687-L734) does. A valid face throws `NotImplemented_nid_no_patch`. The guest face mirrors one glyph slot (`GuestFace::slot`). `SyncSlot` copies the host glyph list head into that single embedded slot, and there is no guest `FT_Done_GlyphSlot` free path. A second host slot has no guest record, and it would move `face->glyph` away from that record. Closes when: guest faces hold a list of glyph slots.
