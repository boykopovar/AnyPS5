# libSceAcm technical debt

### Functional

- [sceAcmBatchJobPriority](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - returning `0x81940006` (`SCE_ACM_ERROR_BAD_POINTER`) when the `batch` pointer or its buffer is unwritable or unreadable via `GuestCopyIn_nid_no_patch` or `GuestCopyOut_nid_no_patch` is a host-safety fallback preserving caller errno; console hardware fault behavior on unmapped memory is unmeasured.

### Silent stubs

- [libSceAcm](../../../core/libs/prx/libSceAcm/Export.cpp) batches execute as silent no-ops, and do not execute encoded panner, FFT, reverb and priority commands (the priority job layout and checks are read from `libSceAcm.sprx`, firmware 13.40, not measured on a console).

### Unknown function info

- [sceAcmBatchStartBuffer, sceAcmBatchStartBuffers and sceAcmContextCreate](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - returning `0x81940006` for unwritable output pointers is a host-safety fallback with console behavior unmeasured.
- [Additional ACM exports](../../../core/libs/prx/libSceAcm/Export.cpp) - the eleven name/NID pairs below are defined exports in the [firmware 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceacm-a3566cc281.jsonl). Their ABIs and behaviour are unknown. Parameterless `int` declarations are import-resolution placeholders, not established signatures; every call throws `NotImplemented_nid_no_patch`. No audio-chain, reverb or telemetry behaviour is implemented.

| Name | NID |
| --- | --- |
| `sceAcmChain_Audio3dPO1_Clear` | `+o6KUwnCB+0` |
| `sceAcmChain_Audio3dPO1_RenderObject` | `0P2PH7lou4g` |
| `sceAcmChain_Audio3dPO1_Activate` | `5pi7bGbV7JM` |
| `sceAcmChain_Audio3dPO1_Output` | `7EGMt3d6RAk` |
| `sceAcm_ConvReverb_SharedIr_2` | `PVx9oQUaDdk` |
| `sceAcmChain_Audio3dPO1_RenderAmbisonicsChannel` | `ZViifnXeAUc` |
| `sceAcmChain_Audio3dPO2_Activate` | `aUfAPcEySHU` |
| `sceAcmChain_Audio3dPO1_RenderPassthroughObject` | `eU9FRHNi+xU` |
| `sceAcmChain_Audio3dPO2_RenderDelay` | `p23f04CznHs` |
| `sceAcmGetTelemetry` | `vQb-lO9SzEI` |
| `sceAcm_ConvReverb_SharedInput_2` | `wM41Cc+ieog` |

- [libSceAcm](../../../core/libs/prx/libSceAcm/Export.cpp) context creation, batch submission and waiting use host identifiers instead of the native device and ioctl requests. Batch identifiers, timeout handling, DSP scheduling and service-side errors remain unimplemented. Panner source parameter and matrix payloads are forwarded by the native library; their DSP interpretation remains unverified.
- [sceAcm_ConvReverb_SharedIr](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - its name and NID are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAcm.c), but no credible prototype was found. It is declared without parameters solely to resolve known imports; this is not an ABI claim
- [`sceAcm_FFT` and `sceAcm_IFFT`](../../../core/libs/prx/libSceAcm/Export.cpp) encode their jobs as `libSceAcm.sprx` does on firmware 13.40, where both exports call one encoder with a direction flag (read from the module's code and compared with that code run on x86-64, not measured on a console). PPSA14632 imports `sceAcm_FFT`; no known title imports `sceAcm_IFFT`. The DSP meaning of the job words is unverified.

- [`sceAcmBatchJobNotification`](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - what its nonzero second argument means and what the DSP later writes into the 128-byte notification object are unverified. The checks and the 16-byte job it appends follow `libSceAcm.sprx` on firmware 13.42 (`0x84d0`, read from disassembly, not run on a console). The firmware does not check that the batch buffer or the notification object is writable, so a bad pointer faults there too.

- [`sceAcm_ConvReverb_SharedInput`](../../../core/libs/prx/libSceAcm/Export.cpp) encodes the reverb jobs as `libSceAcm.sprx` does on firmware 13.40 (read from the module's code and compared with that code run on x86-64, not measured on a console). PPSA01317 calls it during boot with 512-sample blocks. Its input stage appends one `sceAcm_FFT` job of twice the block size (256 points for 128-sample blocks). The meaning of the reverb descriptor's block metadata for the DSP is unverified.
- [`sceAcm_ConvReverb_SharedInput`](../../../core/libs/prx/libSceAcm/Export.cpp) returns `BadPointer` before changing any state for a ring whose `currentBlock` is above `blockCount` (while `availableBlocks` is non-zero), and for a history whose `blockCount` times its slot size exceeds 32 bits. The console's result for these inputs is not measured.

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAcm/Export.cpp) - 4 entries present in both version-labelled images are import-resolution declarations only. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `DvsnlQr4yyk` |
| `Unknown` | `PLNywpi1zR4` |
| `Unknown` | `YpYvibqaqz0` |
| `Unknown` | `ngDRON91+-Y` |

- [Unknown NID `9+f3tPdeyP0`](../../../core/libs/prx/libSceAcm/Export.cpp) (libSceAcm) - its name and the structure it advances are unknown; the parameter and result types come from the body, not a prototype, and nothing is claimed about the object beyond the fields used. Firmware 13.42 `libSceAcm.sprx` (`0x8190`, disassembly only, not run on a console) returns null for a null argument. Otherwise it reads a base pointer at `+0` and 32-bit fields at `+8`, `+0xc` and `+0x10`, raises `+0xc` to at most `+8`, moves `+0x10` to the next slot (0 after an empty state, 0 again at `+8`) and returns the base plus 1024 bytes per slot, with the slot shifted by 8 in 32 bits. Only null is checked; a bad pointer faults there too.

- [Unnamed firmware 13.42 exports](../../../core/libs/prx/libSceAcm/Export.cpp) - 44 function NIDs that firmware 13.42's `system/common/lib/libSceAcm.sprx` exports (read from its export table) are import-resolution declarations only. The [firmware 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceacm-a3566cc281.jsonl) lists the same NIDs as defined function exports with no name, and none is in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv). Names, parameter lists, return types and behavior are unknown; parameterless `int` declarations are placeholders, not verified ABI contracts, and every entry throws through `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `Unknown` | `+GEfQURRfNQ` |
| `Unknown` | `+v8StWu8ce0` |
| `Unknown` | `05vZ-d35iOU` |
| `Unknown` | `29f5DNJTpME` |
| `Unknown` | `2IuXRVnP+E4` |
| `Unknown` | `3odf3m8-sTA` |
| `Unknown` | `3ukt3-zPLo4` |
| `Unknown` | `3v8OJlWuVoI` |
| `Unknown` | `7aoLbYKdti0` |
| `Unknown` | `ADVHzAQRzvI` |
| `Unknown` | `BKv16PL+en4` |
| `Unknown` | `FcV33YZNrZY` |
| `Unknown` | `HRzaZ03MB4c` |
| `Unknown` | `Jd95if4IDsc` |
| `Unknown` | `KUQB9006te4` |
| `Unknown` | `KeUzwnv2znk` |
| `Unknown` | `LOAWrRdCsuI` |
| `Unknown` | `NLMqc049bmY` |
| `Unknown` | `Nir1S45p+k0` |
| `Unknown` | `OXsTjBOwUcs` |
| `Unknown` | `QGrI0Du9IYs` |
| `Unknown` | `RJGnXst7vUQ` |
| `Unknown` | `RbJwbKtBy48` |
| `Unknown` | `S+W03U2x0VU` |
| `Unknown` | `UK03t9UoOWU` |
| `Unknown` | `UnPujhtY1e4` |
| `Unknown` | `VG31URxchBE` |
| `Unknown` | `Zea6TpIIcuw` |
| `Unknown` | `cMDARlsd0IU` |
| `Unknown` | `cQlAR2dYW1U` |
| `Unknown` | `dLDAIKe6QPI` |
| `Unknown` | `l2XvCIV50-Q` |
| `Unknown` | `lbezsUkPojs` |
| `Unknown` | `mXntN6cqsj4` |
| `Unknown` | `n4spvEiudjI` |
| `Unknown` | `ozmQIEUnkNE` |
| `Unknown` | `p8H6QiaoXnY` |
| `Unknown` | `reqnnLJj9lo` |
| `Unknown` | `sAxJPDwmsBg` |
| `Unknown` | `sSI1ZHQh9as` |
| `Unknown` | `sYPAONMjsz4` |
| `Unknown` | `sn178+KL4FI` |
| `Unknown` | `vsxwtmT9mbY` |
| `Unknown` | `zzN06JMvPFc` |
