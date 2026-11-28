# libSceFreeType

## add Functional

- [FT_Load_Sfnt_Table length](../../../core/libs/prx/libSceFreeType/src/Sfnt.cpp) (libSceFreeType) - the guest `length` is read once with `GuestCopyIn_nid_no_patch` after the table lookup; an unreadable length returns `FT_Err_Invalid_Argument` without writing the buffer, with `errno` preserved, because the console's behaviour on unmapped memory is unmeasured. A missing table or face still returns FreeType's code without reading it.
