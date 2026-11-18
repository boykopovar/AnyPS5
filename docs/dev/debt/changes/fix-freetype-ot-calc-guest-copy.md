# libSceFreeTypeOt

## add Functional

- [FT_Vector_Transform_Scaled and ft_synthesize_vertical_metrics guest pointers](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) (libSceFreeTypeOt) - `vector`, `matrix`, and `metrics` are copied in and out through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving caller `errno`; becoming a no-op on unreadable or unwritable pointers is a host-safety fallback, and console behaviour on unmapped memory is unmeasured.
