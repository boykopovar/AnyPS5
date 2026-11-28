# libSceAcm

## add Functional

- [`sceAcm_FFT` and `sceAcm_IFFT`](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - reading the batch structure and the input and output pointer array entries through `GuestCopyIn_nid_no_patch` and committing each appended job word, the reverb job words and descriptors appended for `sceAcm_ConvReverb_SharedInput`, its batch header and the final batch offset through `GuestCopyOut_nid_no_patch`, returning `0x81940006` (`SCE_ACM_ERROR_BAD_POINTER`) with the batch structure unchanged on unmapped or unwritable pointers, is a host-safety fallback; the firmware does not check pointer accessibility and faults there instead, and its behaviour is not measured on a console.
