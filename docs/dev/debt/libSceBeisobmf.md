# libSceBeisobmf technical debt

### Unknown function info

- [libSceBeisobmf pending exports](../../../core/libs/prx/libSceBeisobmf/Export.cpp) - 8 named callable exports remain among 18 exports in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceBeisobmf.c) and firmware 13.60. Signatures and behaviour are unverified; declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.
- [Ten box/decoder accessors](../../../core/libs/prx/libSceBeisobmf/Export.cpp) are implemented from Ghidra 12.1.4 static analysis of `libSceBeisobmf.sprx` in firmware 13.60 with layouts unverified on console. Memory copies use `GuestCopyIn_nid_no_patch` and `GuestCopyOut_nid_no_patch` with defensive fallbacks. `libSceBeisobmf` defines a dedicated subsystem [CMakeLists.txt](../../../core/libs/prx/libSceBeisobmf/CMakeLists.txt) and consolidates box accessor tests.
