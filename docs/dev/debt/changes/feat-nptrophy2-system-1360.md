# libSceNpTrophy2

## add Functional

- [sceNpTrophy2System* handle, context, callback, and icon URI exports](../../../core/libs/prx/libSceNpTrophy2/Export.cpp) - Implemented firmware 13.60 handle/context tables, callback registration slots, `trp2://localfile/%s/...` URI builders, and server-availability gate; `IsServerAvailable` reports `false` without an active NP environment, and callback dispatch remains a no-op until IPC title sync/update events are emulated.
