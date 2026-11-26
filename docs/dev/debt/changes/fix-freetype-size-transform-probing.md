# libSceFreeType

## add Functional

- [FT_New_Size, FT_Activate_Size, FT_Set_Transform guest pointer checks](../../../core/libs/prx/libSceFreeType/Export.cpp) (libSceFreeType) - `FT_New_Size` probes and writes `asize` through `GuestCopyOut_nid_no_patch` (returning `FT_Err_Invalid_Argument` on unwritable output), `FT_Activate_Size` reads `size->internal` through `GuestCopyIn_nid_no_patch` (returning `FT_Err_Invalid_Size_Handle` on unreadable input), and `FT_Set_Transform` reads non-null `matrix` and `delta` through `GuestCopyIn_nid_no_patch` (returning without modifying the face transform on unreadable input), all preserving guest `errno`; console behavior on faulted pointers is unverified.
