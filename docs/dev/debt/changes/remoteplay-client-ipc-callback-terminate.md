# libSceRemotePlayClientIpc

## add Functional

- [sceRemotePlayClientIpcTerminate](../../../core/libs/prx/libSceRemotePlayClientIpc/Export.cpp): returns `0` when disconnected, matching firmware 13.60 cleanup behavior when IPC handles are absent.
- [libSceRemotePlayClientIpcUnknown_2dzIQ1US7Ag_nid_no_patch](../../../core/libs/prx/libSceRemotePlayClientIpc/Export.cpp): takes `void* callback`, returns `0x8122101D` if `nullptr` or already registered, and otherwise stores the callback under mutex and returns `0`.
- [libSceRemotePlayClientIpcUnknown_a3LqZtr60o4_nid_no_patch](../../../core/libs/prx/libSceRemotePlayClientIpc/Export.cpp): clears the stored callback slot under mutex and returns `0`.
