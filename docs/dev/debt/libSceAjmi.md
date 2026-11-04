# libSceAjmi technical debt

### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAjmi/Export.cpp) - 22 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `sceAjmiBatchCancel` | `6lWAGxD8NLs` |
| `sceAjmiBatchWait` | `JJ9g04D344Q` |
| `Unknown` | `4MsUK9kAnKg` |
| `Unknown` | `6fY-ci3au2U` |
| `Unknown` | `9zA4YMlkR9M` |
| `Unknown` | `BD002VBfhuE` |
| `Unknown` | `EYIibEpcHuw` |
| `Unknown` | `J3UwQ5e6sNU` |
| `Unknown` | `LESEGgm133w` |
| `Unknown` | `MTdd8ApwO50` |
| `Unknown` | `QhA7rTjJbro` |
| `Unknown` | `RbLGSlJB6dc` |
| `Unknown` | `Uqtj4RSP-JQ` |
| `Unknown` | `X8sT-xi-dVE` |
| `Unknown` | `YtubBgSeNH0` |
| `Unknown` | `cWZnCK7na5Y` |
| `Unknown` | `dTsNpEt1AqQ` |
| `Unknown` | `dmwcwCeItmE` |
| `Unknown` | `m0xisYC6U2w` |
| `Unknown` | `ofRX8Vq0ss0` |
| `Unknown` | `pfPwiigOE3A` |
| `Unknown` | `r7NMFYcJi08` |

### Unknown function info

- [libSceAjmi pending exports](../../../core/libs/prx/libSceAjmi/Export.cpp) - of the 5 named callable exports identified in the PS5 3.20 export table and cross-checked against firmware 13.60, 2 are declared here, as `sceAjmBatchJobInlineBuffer_nid_disambig1` and `sceAjmBatchJobRunBufferRa_nid_disambig1`, which the NID patcher exports under the names' NIDs, because libSceAjm implements the same names for PS4 titles. Both are firmware 13.42's routines, which write the same bytes as libSceAjm's in an emulator ([sceAjmBatchJobInlineBuffer](https://github.com/boykopovar/AnyPS5/pull/11238#issuecomment-6418132093), [sceAjmBatchJobRunBufferRa](https://github.com/boykopovar/AnyPS5/pull/11238#issuecomment-6442306780)); neither was run on a console. Evidence: [3.20 export list](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAjmi.c).
