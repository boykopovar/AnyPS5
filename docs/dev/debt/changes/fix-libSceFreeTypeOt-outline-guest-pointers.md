# libSceFreeTypeOt

## add Functional

- [FT_Outline_New_Internal and FT_Outline_Done_Internal](../../../core/libs/prx/libSceFreeTypeOt/src/Outline.cpp) (libSceFreeTypeOt) - guest outline destinations and handles are validated via GuestCopyIn_nid_no_patch, GuestWritable_nid_no_patch and GuestCopyOut_nid_no_patch; returning Invalid_Argument on invalid outline pointers is host safety fallback with console behavior unmeasured.
