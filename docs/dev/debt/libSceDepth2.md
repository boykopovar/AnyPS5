# libSceDepth2 technical debt

### Additional callable exports

- [Depth2 export `tLM3AwtzaHs`](../../../core/libs/prx/libSceDepth2/Export.cpp) is a function record with qualifier `L#A` in older and later version-labelled modules. In both, dynamic metadata maps library ID 11 and module ID 0 to `libSceDepth2`. The older module-parameter words are `0x08540001` and `0x03200004`; the later words are `0x13590001` and `0x13600007`. Both have the same 11 function NIDs, whose sorted-LF SHA-256 is `72ab095808199bf39edf98a41b28470660ca1db537e1cfb7be251b4e9d405a98`, using the [static ELF membership procedure](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298). Version labels are not authenticated retail provenance, and the digest identifies the NID set, not the binary or ABI. The export name, signature and behavior are unverified; this declaration throws.


### Unknown function info

- [libSceDepth2 pending exports](../../../core/libs/prx/libSceDepth2/Export.cpp) - 10 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceDepth2.c) and were cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.
