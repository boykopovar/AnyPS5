# libSceVideoOut

## change sceVideoOutAddVrrStatusFlagsPrivilege

- [sceVideoOutAddVrrStatusFlagsPrivilege](../../../core/libs/prx/libSceVideoOut/src/Output.cpp) returns `SCE_VIDEO_OUT_OK` (0) and does nothing: returning OK is intentional and is the answer the only evidence asks for. The host output has no VRR state to report. The name comes from an assert message in a title that requires `SCE_VIDEO_OUT_OK`; the NID hashes to `kP2L8t3j-aM` and the function is called without any arguments, so the return value is the expected answer.
