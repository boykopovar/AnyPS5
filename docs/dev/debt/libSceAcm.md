# libSceAcm technical debt

### Silent stubs

- [libSceAcm](../../../core/libs/prx/libSceAcm/Export.cpp) batches execute as silent no-ops, and do not execute encoded panner and FFT commands. `sceAcm_ConvReverb_SharedInput` returns success without encoding audio commands.

### Unknown function info

- [libSceAcm](../../../core/libs/prx/libSceAcm/Export.cpp) context creation, batch submission and waiting use host identifiers instead of the native device and ioctl requests. Batch identifiers, timeout handling, DSP scheduling and service-side errors remain unimplemented. Panner source parameter and matrix payloads are forwarded by the native library; their DSP interpretation remains unverified.
- [sceAcm_ConvReverb_SharedIr](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - its name and NID are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAcm.c), but no credible prototype was found. It is declared without parameters solely to resolve known imports; this is not an ABI claim
- [`sceAcm_FFT` and `sceAcm_IFFT`](../../../core/libs/prx/libSceAcm/Export.cpp) encode their jobs as `libSceAcm.sprx` does on firmware 13.40, where both exports call one encoder with a direction flag (read from the module's code and compared with that code run on x86-64, not measured on a console). PPSA14632 imports `sceAcm_FFT`; no known title imports `sceAcm_IFFT`. The DSP meaning of the job words is unverified.

- [`sceAcmBatchJobNotification`](../../../core/libs/prx/libSceAcm/Export.cpp) is a parameterless pending declaration for import resolution. Its signature and behavior are unverified; it throws through `NotImplemented_nid_no_patch`.

- The shared-input reverb signature and descriptor fields are recovered from native accesses. Input history indices are mutable; block metadata representation, reverb command encoding and DSP interpretation remain unimplemented in [libSceAcm](../../../core/libs/prx/libSceAcm/Export.cpp).
