# libSceFreeType

## add Functional

- [FT_Get_PS_Font_Value](../../../core/libs/prx/libSceFreeType/src/PsFontValue.cpp) copies retrieved string and widened `FT_Long` values to guest memory through `GuestCopyOut_nid_no_patch` while preserving `errno`, returning `-1` without mutating caller memory when the output buffer is unwritable.
