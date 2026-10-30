# libSceAcm technical debt

### Silent stubs

- [libSceAcm](../../../core/libs/prx/libSceAcm/Export.cpp) batches execute as silent no-ops, and do not execute encoded panner, FFT and reverb commands.

### Unknown function info

- [libSceAcm](../../../core/libs/prx/libSceAcm/Export.cpp) context creation, batch submission and waiting use host identifiers instead of the native device and ioctl requests. Batch identifiers, timeout handling, DSP scheduling and service-side errors remain unimplemented. Panner source parameter and matrix payloads are forwarded by the native library; their DSP interpretation remains unverified.
- [sceAcm_ConvReverb_SharedIr](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - its name and NID are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAcm.c), but no credible prototype was found. It is declared without parameters solely to resolve known imports; this is not an ABI claim
- [`sceAcm_FFT` and `sceAcm_IFFT`](../../../core/libs/prx/libSceAcm/Export.cpp) encode their jobs as `libSceAcm.sprx` does on firmware 13.40, where both exports call one encoder with a direction flag (read from the module's code and compared with that code run on x86-64, not measured on a console). PPSA14632 imports `sceAcm_FFT`; no known title imports `sceAcm_IFFT`. The DSP meaning of the job words is unverified.

- [`sceAcmBatchJobNotification`](../../../core/libs/prx/libSceAcm/Export.cpp) is a parameterless pending declaration for import resolution. Its signature and behavior are unverified; it throws through `NotImplemented_nid_no_patch`.

- [`sceAcm_ConvReverb_SharedInput`](../../../core/libs/prx/libSceAcm/Export.cpp) encodes the reverb jobs as `libSceAcm.sprx` does on firmware 13.40 (read from the module's code and compared with that code run on x86-64, not measured on a console). PPSA01317 calls it during boot with 512-sample blocks. Its input stage appends one `sceAcm_FFT` job of twice the block size (256 points for 128-sample blocks). The meaning of the reverb descriptor's block metadata for the DSP is unverified.
