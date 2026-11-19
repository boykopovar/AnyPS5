# libSceDepth2 technical debt

### Additional callable exports

- [Depth2 export `tLM3AwtzaHs`](../../../core/libs/prx/libSceDepth2/Export.cpp) is a function record with qualifier `L#A` in older and later version-labelled modules. In both, dynamic metadata maps library ID 11 and module ID 0 to `libSceDepth2`. The older module-parameter words are `0x08540001` and `0x03200004`; the later words are `0x13590001` and `0x13600007`. Both have the same 11 function NIDs, whose sorted-LF SHA-256 is `72ab095808199bf39edf98a41b28470660ca1db537e1cfb7be251b4e9d405a98`, using the [static ELF membership procedure](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298). Version labels are not authenticated retail provenance, and the digest identifies the NID set, not the binary or ABI. The export name and signature are unverified; past the checks under Functional, this declaration throws.


### Unknown function info

- [libSceDepth2 pending exports](../../../core/libs/prx/libSceDepth2/Export.cpp) - 10 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceDepth2.c) and were cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention, except the ones under Functional that take the handle, and apart from those answers throw through `NotImplemented_nid_no_patch`.

### Functional

- [sceDepth2Submit, sceDepth2SaveInternalBuffers, sceDepth2SetCameraFactoryCalibrationData, sceDepth2WaitAndExecutePostProcess, sceDepth2GetImage and tLM3AwtzaHs](../../../core/libs/prx/libSceDepth2/Export.cpp) (libSceDepth2) - only the answers for a handle that is not open are implemented: `0x80BB0005`. In firmware 13.42 (read from disassembly, not measured on a console) each looks its first argument up in the module's four-entry handle table under its mutex and returns that value when the entry is not open; only `sceDepth2Initialize`, which throws here, opens one. Before that lookup `GetImage` returns `0x80BB0004` for a non-zero 2nd argument and `0x80BB000A` for a null 3rd, and `tLM3AwtzaHs` `0x80BB000A` for a null 2nd and `0x80BB0000` for a negative 3rd. Only the arguments these checks read are declared; the names are not from a header.
