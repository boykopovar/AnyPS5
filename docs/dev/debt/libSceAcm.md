# libSceAcm technical debt

### Silent stubs

- [libSceAcm](../../../core/libs/prx/libSceAcm/Export.cpp) batches execute as silent no-ops, and do not execute encoded panner and FFT commands. `sceAcm_ConvReverb_SharedInput` and `sceAcmBatchJobPriority` return success without encoding audio commands.

### Unknown function info

- [libSceAcm](../../../core/libs/prx/libSceAcm/Export.cpp) context creation, batch submission and waiting use host identifiers instead of the native device and ioctl requests. Batch identifiers, timeout handling, DSP scheduling and service-side errors remain unimplemented. Panner source parameter and matrix payloads are forwarded by the native library; their DSP interpretation remains unverified.
- [sceAcm_ConvReverb_SharedIr, sceAcm_IFFT](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - their names and NIDs are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAcm.c), but no credible prototypes were found. They are declared without parameters solely to resolve known imports; this is not an ABI claim
- [`sceAcmBatchJobNotification`](../../../core/libs/prx/libSceAcm/Export.cpp) is a parameterless pending declaration for import resolution. Its signature and behavior are unverified; it throws through `NotImplemented_nid_no_patch`.

- The shared-input reverb signature and descriptor fields are recovered from native accesses. Input history indices are mutable; block metadata representation, reverb command encoding and DSP interpretation remain unimplemented in [libSceAcm](../../../core/libs/prx/libSceAcm/Export.cpp).
