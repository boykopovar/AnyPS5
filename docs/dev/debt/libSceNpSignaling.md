# libSceNpSignaling technical debt

### Functional

- [libSceNpSignaling](../../../core/libs/prx/libSceNpSignaling/Export.cpp) (libSceNpSignaling) - offline only, not measured on a PS5: the checks, their order, context ids 1 to 8 and the NpId rule (`term` 0, 3 to 16 characters, the first an ASCII letter or digit, the rest ASCII letters, digits, `_` or `-`) follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/91e6afa69b0a64b0afaa0281b80f8fbb0cd88136/src/core/libraries/np/np_signaling/np_signaling.cpp#L118-L170) and its [state helpers](https://github.com/shadps4-emu/shadPS4/blob/91e6afa69b0a64b0afaa0281b80f8fbb0cd88136/src/core/libraries/np/np_signaling/np_signaling_state.cpp#L120-L170). `sceNpSignalingInitialize` reserves no heap and starts no thread, so its arguments are not checked, and callbacks are never called. `sceNpSignalingActivateConnection` returns `SCE_NP_ERROR_SIGNED_OUT` after shadPS4's checks up to `OWN_NP_ID`, without its rate limit, and creates no connection, so the connection calls answer `CONN_NOT_FOUND`

- [sceNpSignalingDeactivateConnection](../../../core/libs/prx/libSceNpSignaling/Export.cpp) (libSceNpSignaling) - not measured on a PS5: it has the same checks as `sceNpSignalingTerminateConnection` (not initialized, unknown context, then `CONN_NOT_FOUND`), after shadPS4's [call](https://github.com/shadps4-emu/shadPS4/blob/12a4564543ccec527f9723676d2180580d1d7a94/src/core/libraries/np/np_signaling/np_signaling.cpp#L378-L400). No connection is ever created, so none can be deactivated.

- [sceNpSignalingGetConnectionStatus](../../../core/libs/prx/libSceNpSignaling/Export.cpp) (libSceNpSignaling) - not measured on a PS5: this and the eleven other offline queries in `Api.hpp` take signatures and checks from [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/d1d0b58cf94e5bfb6a2be406e140dcf1e314853f/src/core/libraries/np/np_signaling/np_signaling.cpp#L172-L819) with no connection or request. `CreateContextA` and `ActivateConnectionA` return `SCE_NP_ERROR_SIGNED_OUT` as libSceNpManager signs no one in, so no context has an account and `GetConnectionFromPeerAddressA` and `GetPeerNetInfoA` answer `INVALID_ARGUMENT`. Connections read inactive and are not found, requests and results are not found, counts are 0. `GetLocalNetInfo`, `GetMemoryInfo` and `GetPeerNetInfo` still throw.

### Additional callable export inventory

- [Additional callable exports](../../../core/libs/prx/libSceNpSignaling/Export.cpp) - 3 entries have import-resolution declarations only. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. No runtime support, implemented-function increase or title unblock is claimed.

| Name | NID |
| --- | --- |
| `sceNpSignalingGetLocalNetInfo` | `U8AQMlOFBc8` |
| `sceNpSignalingGetMemoryInfo` | `tOpqyDyMje4` |
| `sceNpSignalingGetPeerNetInfo` | `zFgFHId7vAE` |
