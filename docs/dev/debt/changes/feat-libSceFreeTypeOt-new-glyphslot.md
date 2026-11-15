# libSceFreeTypeOt

## add Functional

- [FT_New_GlyphSlot](../../../core/libs/prx/libSceFreeTypeOt/src/GlyphSlot.cpp) (libSceFreeTypeOt) - a null face returns Invalid_Face_Handle (0x23), and a face without a driver returns Invalid_Argument, as the 2.4.12 [ftobjs.c](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftobjs.c#L687-L734) does. A valid face throws `NotImplemented_nid_no_patch`. The guest face mirrors one glyph slot (`GuestFace::slot`). A second host slot has no guest record. It would also move `face->glyph` away from that record. Closes when: guest faces mirror a list of glyph slots.
