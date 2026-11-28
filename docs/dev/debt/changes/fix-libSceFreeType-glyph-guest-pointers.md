# libSceFreeType

## add Unknown function info

- [FT_Get_Glyph, FT_Glyph_Copy, FT_Glyph_Get_CBox, FreeTypeNewGlyph, FreeTypeGlyphMutationToBitmapHost, FreeTypeGlyphMutationTransformHost, FreeTypeGlyphMutationStrokeHost, FreeTypeGlyphMutationStrokeBorderHost](../../../core/libs/prx/libSceFreeType/src/Glyph.cpp) (libSceFreeType) - unreadable or unwritable guest slot, library, outline, glyph, target, bbox, origin, matrix, and delta pointers are validated through `GuestArenaPrepareHostRead_nid_no_patch` / `GuestReadable_nid_no_patch` / `GuestCopyIn_nid_no_patch` and `GuestArena::HostWrite` / `GuestWritable_nid_no_patch` / `GuestCopyOut_nid_no_patch` while preserving `errno`, returning FreeType error codes (or returning early for void `FT_Glyph_Get_CBox`) instead of dereferencing guest memory directly; console fault behavior for invalid guest pointers is not measured.
