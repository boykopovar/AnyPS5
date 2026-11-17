# libSceFreeType

## add Functional

- [FT_Vector_Transform, FT_Matrix_Multiply, FT_Matrix_Invert](../../../core/libs/prx/libSceFreeType/src/Geometry.cpp) (libSceFreeType) - vector and matrix operands and outputs are validated and transferred via GuestCopyIn_nid_no_patch, GuestWritable_nid_no_patch and GuestCopyOut_nid_no_patch; invalid pointer rejection via throwing or returning FT_Err_Invalid_Argument is host safety fallback with console behavior unmeasured.
