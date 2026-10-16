# libSceVoice technical debt

### Unknown function info

### Functional

- [sceVoiceWriteToIPort](../../../core/libs/prx/libSceVoice/Export.cpp) validates the data and size pointers and the input-port type, then consumes zero bytes because the host has no voice encoder or input transport. [sceVoiceEnableChat](../../../core/libs/prx/libSceVoice/Export.cpp) and [sceVoiceDisableChat](../../../core/libs/prx/libSceVoice/Export.cpp) return success because the host has no separate voice-chat mixer.

- [sceVoiceResetPort](../../../core/libs/prx/libSceVoice/Export.cpp) (libSceVoice) - the `(uint32_t portId)` signature is the PS Vita one in [VitaSDK](https://github.com/vitasdk/vita-headers/blob/ae5808f3105c42fa3373ba4ab64ab54e2e3ca848/include/psp2/voice.h#L503-L514) and the PS3 `cellVoiceResetPort` in [RPCS3](https://github.com/RPCS3/rpcs3/blob/be69c054eeac57e2e671697a463473d37fe5a7b2/rpcs3/Emu/Cell/Modules/cellVoice.cpp#L493-L510); that the PS5 library takes the same argument is assumed. A reset clears a port's buffered data, and ports here buffer nothing (`sceVoiceReadFromOPort` returns 0 bytes and `sceVoiceWriteToIPort` throws), so it only checks that the library is initialized and the port exists, as RPCS3 does, and keeps the port's connections, volume and mute flag. An uninitialized library or an unknown port throws like the other port calls; the console's error codes are not measured. PPSA23566 imports it
