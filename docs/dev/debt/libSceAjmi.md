# libSceAjmi technical debt

### Unknown function info

- [libSceAjmi pending exports](../../../core/libs/prx/libSceAjmi/Export.cpp) - 5 named callable exports were identified in the PS5 3.20 export table and cross-checked against firmware 13.60. Signatures and behavior remain unverified; declarations use the unknown-signature convention and throw through `NotImplemented_nid_no_patch`. Evidence: [3.20 export list](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAjmi.c).
