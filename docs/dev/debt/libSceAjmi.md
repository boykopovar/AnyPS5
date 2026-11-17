# libSceAjmi technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAjmi/Export.cpp) - 20 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. The remaining 18 missing from that catalogue are named by NID hash; 17 share a `sceAjm` twin's 13.42 address. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `sceAjmiBatchCancel` | `6lWAGxD8NLs` |
| `sceAjmiBatchWait` | `JJ9g04D344Q` |
| `sceAjmiModuleUnregister` | `6fY-ci3au2U` |
| `sceAjmiBatchStartBuffer` | `9zA4YMlkR9M` |
| `sceAjmiFinalize` | `BD002VBfhuE` |
| `sceAjmiStrError` | `EYIibEpcHuw` |
| `sceAjmiBatchJobControlBufferRa` | `J3UwQ5e6sNU` |
| `sceAjmiInstanceSwitch` | `LESEGgm133w` |
| `sceAjmiDecAt9ParseConfigData` | `MTdd8ApwO50` |
| `sceAjmiInitialize` | `QhA7rTjJbro` |
| `sceAjmiBatchErrorDump` | `RbLGSlJB6dc` |
| `sceAjmiInstanceCodecType` | `Uqtj4RSP-JQ` |
| `sceAjmiMemoryUnregister` | `X8sT-xi-dVE` |
| `sceAjmiInstanceDestroy` | `YtubBgSeNH0` |
| `sceAjmiDecMp3ParseFrame` | `cWZnCK7na5Y` |
| `sceAjmiMemoryRegister` | `dTsNpEt1AqQ` |
| `sceAjmiModuleRegister` | `dmwcwCeItmE` |
| `sceAjmiInstanceCreate` | `ofRX8Vq0ss0` |
| `sceAjmiInstanceExtend` | `pfPwiigOE3A` |
| `sceAjmiBatchJobRunSplitBufferRa` | `r7NMFYcJi08` |

### Unknown function info

- [libSceAjmi pending exports](../../../core/libs/prx/libSceAjmi/Export.cpp) - of the 5 named callable exports identified in the PS5 3.20 export table and cross-checked against firmware 13.60, 4 are declared here, as `sceAjmBatchJobInlineBuffer_nid_disambig1`, `sceAjmBatchJobRunBufferRa_nid_disambig1`, `sceAjmBatchJobControlBufferRa_nid_disambig1` and `sceAjmBatchJobRunSplitBufferRa_nid_disambig1`, exported under the names' NIDs because libSceAjm implements the same names. All four are firmware 13.42's routines, which write the same bytes as libSceAjm's in an emulator ([inline buffer](https://github.com/boykopovar/AnyPS5/pull/11238#issuecomment-6418132093), [run buffer](https://github.com/boykopovar/AnyPS5/pull/11238#issuecomment-6442306780), [control and split](https://github.com/boykopovar/AnyPS5/pull/12372#issuecomment-6482366782)); none ran on a console. Evidence: [3.20 export list](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAjmi.c).
- [sceAjmBatchJobInlineBuffer_nid_disambig1](../../../core/libs/prx/libSceAjmi/Export.cpp) - a data size above 0xFFFFFFF8 (the largest 8-byte multiple a 32-bit job size word holds) throws `std::invalid_argument` before anything is written, since the job word cannot describe the copy. The console's result for such a size is not measured.

### Batch builder aliases

- Direct-console operation and other firmware versions remain unverified for [`sceAjmiBatchJobInlineBuffer` and `sceAjmiBatchJobRunBufferRa`](../../../core/libs/prx/libSceAjmi/Export.cpp). Their signatures and forwarding follow the [13.42 symbol-table report](https://github.com/boykopovar/AnyPS5/pull/15495): each shares its entry address with the implemented `sceAjm*` export, at 0xb30 and 0xc50 in `libSceAjmi.sprx` (SHA-256 `ff0dc985c1f7a6b25a766904e3a535b55e44d0d4b8872329d2bd6c019491df0b`). The same-module routines were measured under an emulator, not on a console, as cited above. Invalid pointers and insufficient batch capacity remain unchecked by those builders, and `sceAjmiBatchJobInlineBuffer` inherits the oversize-data throw above.
