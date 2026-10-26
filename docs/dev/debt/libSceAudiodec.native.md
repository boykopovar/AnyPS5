# libSceAudiodec.native technical debt

### Unknown function info

- [libSceAudiodec.native pending exports](../../../core/libs/prx/libSceAudiodec.native/Export.cpp) - 14 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAudiodec.native.c) and were cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.
