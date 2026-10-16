# libSceVoice technical debt

### Functional

- [sceVoiceEnableChat, sceVoiceDisableChat](../../../core/libs/prx/libSceVoice/Export.cpp) (libSceVoice) take no arguments and return 0 once the library is initialized, like [sceAudioOut2EnableChat](../../../core/libs/prx/libSceAudioOut/src/AudioOut2System.cpp): they only record whether chat audio is allowed. The host mixes no chat audio and Voice has no chat pipeline, so nothing else changes. Whether the console fails them without a chat session, when repeated, or what error it uses before `sceVoiceInit` is unknown; an uninitialized library throws like the other Voice calls. PPSA34015 imports them

### Unknown function info

- [sceVoiceResetPort](../../../core/libs/prx/libSceVoice/Export.cpp) (libSceVoice) - the `(uint32_t portId)` signature is the PS Vita one in [VitaSDK](https://github.com/vitasdk/vita-headers/blob/ae5808f3105c42fa3373ba4ab64ab54e2e3ca848/include/psp2/voice.h#L503-L514) and the PS3 `cellVoiceResetPort` in [RPCS3](https://github.com/RPCS3/rpcs3/blob/be69c054eeac57e2e671697a463473d37fe5a7b2/rpcs3/Emu/Cell/Modules/cellVoice.cpp#L493-L510); that the PS5 library takes the same argument is assumed. A reset clears a port's buffered data, and ports here buffer nothing (`sceVoiceReadFromOPort` returns 0 bytes and `sceVoiceWriteToIPort` throws), so it only checks that the library is initialized and the port exists, as RPCS3 does, and keeps the port's connections, volume and mute flag. An uninitialized library or an unknown port throws like the other port calls; the console's error codes are not measured. PPSA23566 imports it
- [sceVoiceGetResourceInfo](../../../core/libs/prx/libSceVoice/Export.cpp) (libSceVoice) - declared without parameters; signature and result layout unknown, so it throws. PPSA34015 imports it
