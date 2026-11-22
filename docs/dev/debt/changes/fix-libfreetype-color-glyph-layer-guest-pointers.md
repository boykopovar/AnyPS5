# libfreetype

## add Functional

- [FT_Get_Color_Glyph_Layer](../../../core/libs/prx/libfreetype/src/Color.cpp) (libfreetype) - glyphIndex, colorIndex, and iterator guest pointers are validated with GuestWritable_nid_no_patch and written through GuestCopyOut_nid_no_patch with caller errno preserved; returning 0 on unwritable pointer is host safety fallback with console behavior unmeasured.
