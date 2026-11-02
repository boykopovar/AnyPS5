# libSceSystemTts technical debt

### Additional callable exports

- [SystemTts export `aIz2UUh6oi0`](../../../core/libs/prx/libSceSystemTts/Export.cpp) is a function record with qualifier `L#A` in older and later version-labelled modules. In both, dynamic metadata maps library ID 11 and module ID 0 to `libSceSystemTts`. Older module words are `0x08540001` and `0x03200004`; later words are `0x13590001` and `0x13600007`. The older image has seven function NIDs with sorted-LF SHA-256 `64564168e4e9e9ac73985b56e5de18ae073f1698cbdfca177c2add4b22f9323f`, using the [static ELF membership procedure](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298). Version labels do not authenticate retail provenance, and the digest identifies the NID set only. The export name, signature and behavior are unverified; this declaration throws.


### Unknown function info

- [sceSystemTtsConvertText, sceSystemTtsPauseResume, sceSystemTtsRepeat, sceSystemTtsWait](../../../core/libs/prx/libSceSystemTts/Export.cpp) have unknown signatures and behaviour. The parameterless `int` declarations are import-resolution placeholders, not confirmed ABI signatures, and throw through `NotImplemented_nid_no_patch`. Only the names and NIDs are established by the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscesystemtts-8f4923bcd1.jsonl).
- [sceSystemTtsCancel, sceSystemTtsIsAccessibilityAvailable, sceSystemTtsRegisterCallback, sceSystemTtsSpeakRequest and sceSystemTtsUnregisterCallback](../../../core/libs/prx/libSceSystemTts/Export.cpp) (libSceSystemTts) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSystemTts.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified
