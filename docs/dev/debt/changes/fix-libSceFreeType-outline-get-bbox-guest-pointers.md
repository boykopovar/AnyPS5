# libSceFreeType

## add Functional

- [FT_Outline_Get_BBox](../../../core/libs/prx/libSceFreeType/src/CurveBounds.cpp) (libSceFreeType) - boundingBox output pointer is validated with GuestWritable_nid_no_patch and written through GuestCopyOut_nid_no_patch; returning FT_Err_Invalid_Argument on unwritable pointer is host safety fallback with console behavior unmeasured.
