# libSceFreeType

## add Functional

- [FT_New_Size_nid_postfix, FT_Activate_Size_nid_postfix, and FT_Set_Transform_nid_postfix](../../../../core/libs/prx/libSceFreeType/Export.cpp) (libSceFreeType) - probing `asize`, `size`, `matrix`, and `delta` via `GuestCopyIn_nid_no_patch` / `GuestCopyOut_nid_no_patch` and returning `FT_Err_Invalid_Argument` / `FT_Err_Invalid_Size_Handle` or returning early while preserving `errno` on invalid pointers is a host-safety fallback; console behaviour on unmapped pointers is unmeasured.
