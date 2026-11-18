# libSceSystemTts

## change [sceSystemTtsConvertText, sceSystemTtsPauseResume, sceSystemTtsRepeat, sceSystemTtsWait]

- [sceSystemTtsConvertText and sceSystemTtsRepeat](../../../core/libs/prx/libSceSystemTts/Export.cpp) have unknown signatures and behaviour. The parameterless `int` declarations are import-resolution placeholders, not confirmed ABI signatures, and throw through `NotImplemented_nid_no_patch`. `sceSystemTtsPauseResume` and `sceSystemTtsWait` return 0: no host tts engine is attached, so there is nothing to pause, resume or wait for. Only the names and NIDs are established by the [13.60 export inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libscesystemtts-8f4923bcd1.jsonl).

## change [sceSystemTtsCancel, sceSystemTtsIsAccessibilityAvailable, sceSystemTtsRegisterCallback, sceSystemTtsSpeakRequest and sceSystemTtsUnregisterCallback]

- [sceSystemTtsRegisterCallback and sceSystemTtsSpeakRequest](../../../core/libs/prx/libSceSystemTts/Export.cpp) (libSceSystemTts) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSystemTts.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified. `sceSystemTtsCancel`, `sceSystemTtsIsAccessibilityAvailable` and `sceSystemTtsUnregisterCallback` return 0: no host tts engine is attached, so cancel has nothing to cancel, accessibility reports unavailable, and unregister has no callback to remove.
