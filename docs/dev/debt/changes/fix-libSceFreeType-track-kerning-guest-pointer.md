# libSceFreeType

## add Functional

- [FT_Get_Track_Kerning](../../../core/libs/prx/libSceFreeType/src/TrackKerning.cpp) (libSceFreeType) - kerning output pointer is validated with GuestWritable_nid_no_patch and written through GuestCopyOut_nid_no_patch; returning FT_Err_Invalid_Argument on unwritable pointer is host safety fallback with console behavior unmeasured.
