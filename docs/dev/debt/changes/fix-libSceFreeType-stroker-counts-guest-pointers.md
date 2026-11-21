# libSceFreeType

## add Functional

- [FT_Stroker_New](../../../core/libs/prx/libSceFreeType/src/Stroker.cpp) (libSceFreeType) - stroker output pointer is validated with GuestWritable_nid_no_patch and copied out via GuestCopyOut_nid_no_patch; returning FT_Err_Invalid_Argument on unmapped destination is host safety fallback with console behavior unmeasured.
- [FT_Stroker_GetCounts](../../../core/libs/prx/libSceFreeType/src/Stroker.cpp) (libSceFreeType) - points and contours output pointers are validated with GuestWritable_nid_no_patch and copied out via GuestCopyOut_nid_no_patch; returning FT_Err_Invalid_Argument on unmapped destination is host safety fallback with console behavior unmeasured.
- [FT_Stroker_GetBorderCounts](../../../core/libs/prx/libSceFreeType/src/Stroker.cpp) (libSceFreeType) - points and contours output pointers are validated with GuestWritable_nid_no_patch and copied out via GuestCopyOut_nid_no_patch; returning FT_Err_Invalid_Argument on unmapped destination is host safety fallback with console behavior unmeasured.
