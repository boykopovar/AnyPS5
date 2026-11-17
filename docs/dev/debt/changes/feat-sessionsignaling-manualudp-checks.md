# libSceNpSessionSignaling

## change [sceNpSessionSignalingManualUdpAbortConnection, sceNpSessionSignalingManualUdpCreateJoin, sceNpSessionSignalingManualUdpEstablishConnection and sceNpSessionSignalingManualUdpJoin]

- [sceNpSessionSignalingManualUdpAbortConnection, sceNpSessionSignalingManualUdpCreateJoin, sceNpSessionSignalingManualUdpEstablishConnection and sceNpSessionSignalingManualUdpJoin](../../../core/libs/prx/libSceNpSessionSignaling/Export.cpp) (libSceNpSessionSignaling) - take `(const void* param, uint32_t* id, uint32_t* extra)`, `(const void* param, const char* name, uint32_t* id, uint32_t* extra)`, `(uint32_t contextId, uint32_t value)` and `(uint32_t contextId)` and make firmware 13.42's checks ([reading](https://github.com/boykopovar/AnyPS5/issues/21755#issuecomment-6932927884), disassembly only, not run on a console): NOT_INITIALIZED, then INVALID_ARGUMENT for a null `param`, `name` or `id` or an empty `name` (`extra` may be null), and CONTEXT_NOT_FOUND for an unknown context in EstablishConnection and AbortConnection. Past those the console calls the manager's virtual methods, which weren't read, so they throw. An unreadable `name` gives INVALID_ARGUMENT, a host-safety fallback

# libSceNpRemotePlaySessionSignaling

## change [sceNpRemotePlaySessionSignalingAbortConnection, sceNpRemotePlaySessionSignalingEstablishConnection, sceNpRemotePlaySessionSignalingManualUdpCreateJoinSession and sceNpRemotePlaySessionSignalingManualUdpJoinSession]

- [sceNpRemotePlaySessionSignalingAbortConnection, sceNpRemotePlaySessionSignalingEstablishConnection, sceNpRemotePlaySessionSignalingManualUdpCreateJoinSession and sceNpRemotePlaySessionSignalingManualUdpJoinSession](../../../core/libs/prx/libSceNpRemotePlaySessionSignaling/Export.cpp) (libSceNpRemotePlaySessionSignaling) - forward to `sceNpSessionSignalingManualUdpAbortConnection`, `...EstablishConnection`, `...CreateJoin` and `...Join`, as firmware 13.42's module does with a tail jump to each ([reading](https://github.com/boykopovar/AnyPS5/issues/21846#issuecomment-6939330083)), so they share those functions' arguments, checks and gaps
