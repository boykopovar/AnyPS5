# libSceFreeType

## add Functional

- [FT_Get_Advance and FT_Get_Advances guest outputs](../../../core/libs/prx/libSceFreeType/src/Advances.cpp) (libSceFreeType) - advance output ranges are validated with `GuestArena::HostWrite` and `GuestWritable_nid_no_patch` and read or written through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while keeping `errno`; returning `FT_Err_Invalid_Argument` on an unwritable non-empty destination is a host safety fallback with console behavior unmeasured.
