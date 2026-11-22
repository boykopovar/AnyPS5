# libSceFreeTypeOt

## add Functional

- [FT_Vector_Transform_Scaled, ft_synthesize_vertical_metrics and ft_service_list_lookup guest pointers](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) (libSceFreeTypeOt) - guest vectors, matrices, glyph metrics, service descriptor entries and service ID strings are transferred through `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` while preserving `errno`, rejecting inaccessible or unwritable pointers via `std::invalid_argument` as a host-safety fallback with console behavior unmeasured.
