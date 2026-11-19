# libSceAjm.native

## change sceAjmBatchErrorDump

- [sceAjmBatchErrorDump](../../../core/libs/prx/libSceAjm.native/src/Ajm.cpp) (libSceAjm.native) - ignores the batch information and zeroes the error record via GuestCopyOut_nid_no_patch with caller errno preserved, returning SCE_AJM_ERROR_INVALID_PARAMETER for unwritable pointers. Borderlands 4, Forspoken, Metal Gear Solid 4, MXGP 26 and Super Meat Boy 3D call it
