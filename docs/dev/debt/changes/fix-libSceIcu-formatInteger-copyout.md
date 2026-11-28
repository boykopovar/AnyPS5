# libSceIcu

## add Functional

- [T_CString_integerToString and T_CString_int64ToString guest buffer outputs](../../../core/libs/prx/libSceIcu/CString.cpp) (libSceIcu) - integer formatted string outputs are copied to the destination buffer using GuestCopyOut_nid_no_patch; throwing APS5_INVALID_ARG_EX on unwritable destination is host safety fallback with console behaviour unmeasured.
