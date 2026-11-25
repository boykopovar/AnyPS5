# libSceFreeType

## add Functional

- [FT_Outline_Get_BBox outline input](../../../core/libs/prx/libSceFreeType/src/CurveBounds.cpp) (libSceFreeType) - the outline header and its point and tag arrays are read once per call through GuestCopyIn_nid_no_patch; returning FT_Err_Invalid_Argument on an unreadable outline, point or tag array is host safety fallback with console behavior unmeasured.
