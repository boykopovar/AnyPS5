# libSceAjmi technical debt

### Functional

- [libSceAjmi pending exports](../../../core/libs/prx/libSceAjmi/Export.cpp) - of the 5 named callable exports identified in the PS5 3.20 export table and cross-checked against firmware 13.60, 2 are declared here, as `sceAjmBatchJobInlineBuffer_nid_disambig1` and `sceAjmBatchJobRunBufferRa_nid_disambig1`, which the NID patcher exports under the names' NIDs, because libSceAjm implements the same names for PS4 titles. `sceAjmBatchJobInlineBuffer` and `sceAjmBatchJobRunBufferRa` match firmware 13.42's routines and libSceAjm's batch builder format ([emulated run](https://github.com/boykopovar/AnyPS5/pull/11238#issuecomment-6418132093)). Evidence: [3.20 export list](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAjmi.c).
