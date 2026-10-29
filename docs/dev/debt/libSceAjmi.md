# libSceAjmi technical debt

### Unknown function info

- [libSceAjmi pending exports](../../../core/libs/prx/libSceAjmi/Export.cpp) - of the 5 named callable exports identified in the PS5 3.20 export table and cross-checked against firmware 13.60, 2 are declared here, as `sceAjmBatchJobInlineBuffer_nid_disambig1` and `sceAjmBatchJobRunBufferRa_nid_disambig1`, which the NID patcher exports under the names' NIDs, because libSceAjm implements the same names for PS4 titles. `sceAjmBatchJobInlineBuffer` is firmware 13.42's routine, the same as libSceAjm's ([emulated run](https://github.com/boykopovar/AnyPS5/pull/11238#issuecomment-6418132093)). `sceAjmBatchJobRunBufferRa` still throws through `NotImplemented_nid_no_patch` with the unknown-signature convention: the same comment reports its code matches libSceAjm's, but it was not run. Evidence: [3.20 export list](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAjmi.c).
