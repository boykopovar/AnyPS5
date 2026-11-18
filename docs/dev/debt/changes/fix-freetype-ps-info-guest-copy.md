# libSceFreeType

## add Functional

- [FT_Get_PS_Font_Info](../../../core/libs/prx/libSceFreeType/src/PostScriptInfo.cpp) populates a local `GuestPSFontInfo` record and copies it out through `GuestCopyOut_nid_no_patch` while preserving `errno`, returning `FT_Err_Invalid_Argument` without mutating caller memory when the output pointer is unwritable.
