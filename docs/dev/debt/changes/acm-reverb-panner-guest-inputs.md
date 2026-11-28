# libSceAcm

## add Functional

- [sceAcm_ConvReverb_SharedInput and sceAcm_Panner](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - reading the input, history, filter, output, gains, source, parameter, matrix and channel structures and arrays through `GuestCopyIn_nid_no_patch` and writing the input structure, the batch header and the panner command block through `GuestCopyOut_nid_no_patch`, returning `0x81940006` while preserving `errno` on inaccessible guest pointers, is a host-safety fallback; the firmware faults on those pointers instead and its behaviour is not measured on a console.
