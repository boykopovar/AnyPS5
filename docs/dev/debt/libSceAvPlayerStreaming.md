# libSceAvPlayerStreaming technical debt

### Unknown function info

- [libSceAvPlayerStreaming pending exports](../../../core/libs/prx/libSceAvPlayerStreaming/Export.cpp) - 33 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvPlayerStreaming.c) and were cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.

- [sceAvPlayerStreamingRegisterVprinf](../../../core/libs/prx/libSceAvPlayerStreaming/Export.cpp) throw through `NotImplemented_nid_no_patch`. Names and NIDs are bound by the [13.60 firmware inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceavplayerstreaming-27cc7363f9.jsonl); signatures and behaviour remain unknown. Parameterless `int` declarations are import-resolution placeholders, not confirmed ABI signatures. Their service behaviour is unimplemented.
