# libSceFreeTypeOt

## add Functional

- [FT_Outline_Transform, FT_Outline_Embolden and FT_Outline_EmboldenXY](../../../core/libs/prx/libSceFreeTypeOt/src/OutlineTransform.cpp) (libSceFreeTypeOt) - the outline header, matrix, point and contour arrays are read once per call through GuestCopyIn_nid_no_patch and the points are written back once through GuestCopyOut_nid_no_patch; throwing std::invalid_argument from FT_Outline_Transform and returning Invalid_Argument from the embolden calls on unreadable or unwritable guest memory is host safety fallback with console behavior unmeasured.
