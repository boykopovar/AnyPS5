# libSceDepth technical debt

### Unknown function info

- [Depth export `unL9sWu4IIw`](../../../core/libs/prx/libSceDepth/Export.cpp) is a function record with qualifier `G#A` in older and later version-labelled modules. In both, dynamic metadata maps library ID 6 and module ID 0 to `libSceDepth`. The older module-parameter words are `0x08540001` and `0x03200004`; the later words are `0x13590001` and `0x13600007`. Both have the same 39 function NIDs, whose sorted-LF SHA-256 is `a56e046304aaf76b3193f5b4fc0a57c4660bff9b05255e51149891461c23becc`, using the [static ELF membership procedure](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298). Version labels are not authenticated retail provenance, and the digest identifies the NID set, not the binary or ABI. The export name, signature and behavior are unverified; this declaration throws.

- [_sceDepthCalcHeteroAdjustParam, _sceDepthHeadCandidateTrackerGetInformation, _sceDepthHeadCandidateTrackerSetInformation, _sceDepthSetCameraFactoryCalibrationData, _sceDepthSetHeteroAdjustParam, _sceDepthSetHeteroMode](../../../core/libs/prx/libSceDepth/Export.cpp) (libSceDepth) - signatures and behavior are unverified; these parameterless declarations only resolve imports and throw through `NotImplemented_nid_no_patch`.

- [libSceDepth pending exports](../../../core/libs/prx/libSceDepth/Export.cpp) - 32 named callable exports are listed in the [PS5 3.20 export table](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceDepth.c) and were cross-checked against the same module in PS5 firmware 13.60. Their signatures and behaviour are unverified; the declarations use the no-parameter unknown-signature convention and throw through `NotImplemented_nid_no_patch`.
