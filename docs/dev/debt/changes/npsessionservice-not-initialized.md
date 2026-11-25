# libSceNpGameIntent

## change [sceNpSessionServiceInitialize, sceNpSessionServiceSendPlayerSessionRequestToJoin

- [sceNpSessionServiceInitialize, sceNpSessionServiceSendPlayerSessionRequestToJoin and sceNpSessionServiceTerminate](../../../core/libs/prx/libSceNpGameIntent/Export.cpp) (libSceNpGameIntent) - names and NIDs are present in the PS5 13.60 export inventory. Their signatures and behavior are unknown; they are declared without parameters solely to resolve known imports, and make no ABI claim. `sceNpSessionServiceInitialize` throws; the other two answer as the "sceNpSessionService calls before initialization" entry describes.

## add Unknown function info

- [sceNpSessionService calls before initialization](../../../core/libs/prx/libSceNpGameIntent/Export.cpp) (libSceNpGameIntent) - only the state before `sceNpSessionServiceInitialize` is modelled: `sceNpSessionServiceGetJoinedPartySession`, `sceNpSessionServiceGetRunningGamePlayerSession`, `sceNpSessionServiceSendPlayerSessionInvitations`, `sceNpSessionServiceSendPlayerSessionRequestToJoin` and `sceNpSessionServiceTerminate` return `0x8055ca02` whatever their arguments, as firmware 13.42 `libSceNpGameIntent.sprx` does while its byte `0xc128` (zero-fill memory, set only by `sceNpSessionServiceInitialize` at `0x2f07`, cleared by `sceNpSessionServiceTerminate` at `0x2f41`) is 0; run under a CPU emulator, not measured on a console. `sceNpSessionServiceInitialize` throws here, so they always give that answer; they must follow the byte once it is implemented. Prototypes, the initialized behaviour and the name of `0x8055ca02` are unknown
