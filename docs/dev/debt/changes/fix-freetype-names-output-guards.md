# libSceFreeType

## add Functional

- [FT_Get_Glyph_Name, FT_Get_Sfnt_Name](../../../core/libs/prx/libSceFreeType/src/Names.cpp) validate and populate guest outputs through `GuestWritable_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, returning `FT_Err_Invalid_Argument` without mutating caller memory when an output pointer is unreadable or unwritable.
