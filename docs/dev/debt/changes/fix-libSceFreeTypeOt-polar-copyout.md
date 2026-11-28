# libSceFreeTypeOt

## add Functional

- [sceFreeTypeOtVectorPolarize, sceFreeTypeOtVectorFromPolar, sceFreeTypeOtVectorRotate and sceFreeTypeOtVectorLength guest pointers](../../../core/libs/prx/libSceFreeTypeOt/src/Polar.cpp) (libSceFreeTypeOt) - input vectors and output lengths, angles and rotated coordinates are copied and validated using GuestCopyIn_nid_no_patch, GuestCopyOut_nid_no_patch and GuestWritable_nid_no_patch; throwing NotImplemented_nid_no_patch on invalid pointer access is host safety fallback with console behaviour unmeasured.
