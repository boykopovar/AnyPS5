# libSceFreeTypeOt

## add Functional

- [FT_Match_Size_nid_postfix size index output](../../../core/libs/prx/libSceFreeTypeOt/src/SizeMetrics.cpp) (libSceFreeTypeOt) - size index output is validated with GuestWritable_nid_no_patch upfront and written using GuestCopyOut_nid_no_patch; returning FT_Err_Invalid_Argument on unwritable destination is host safety fallback with console behaviour unmeasured.
