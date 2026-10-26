# libSceAjmi technical debt

### Unknown function info

- [libSceAjmi pending exports](../../../core/libs/prx/libSceAjmi/Export.cpp) - of the 5 named callable exports identified in the PS5 3.20 export table and cross-checked against firmware 13.60, 2 are declared here: `sceAjmBatchJobInlineBuffer` and `sceAjmBatchJobRunBufferRa`, the latter as `sceAjmBatchJobRunBufferRa_nid_disambig1`, which the NID patcher exports under the name's NID, because libSceAjm implements the same name for PS4 titles; a title importing it from libSceAjmi still gets the not-implemented error at call time. Signatures and behavior remain unverified; the declarations use the unknown-signature convention and throw through `NotImplemented_nid_no_patch`. Evidence: [3.20 export list](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAjmi.c).
