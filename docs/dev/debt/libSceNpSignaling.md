# libSceNpSignaling technical debt

### Functional

- [libSceNpSignaling](../../../core/libs/prx/libSceNpSignaling/Export.cpp) (libSceNpSignaling) - offline only, not measured on a PS5: the checks, their order, context ids 1 to 8 and the NpId rule (`term` 0, 3 to 16 characters, the first an ASCII letter or digit, the rest ASCII letters, digits, `_` or `-`) follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/91e6afa69b0a64b0afaa0281b80f8fbb0cd88136/src/core/libraries/np/np_signaling/np_signaling.cpp#L118-L170) and its [state helpers](https://github.com/shadps4-emu/shadPS4/blob/91e6afa69b0a64b0afaa0281b80f8fbb0cd88136/src/core/libraries/np/np_signaling/np_signaling_state.cpp#L120-L170). `sceNpSignalingInitialize` reserves no heap and starts no thread, so its arguments are not checked, and callbacks are never called. `sceNpSignalingActivateConnection` returns `SCE_NP_ERROR_SIGNED_OUT` after shadPS4's checks up to `OWN_NP_ID`, without its rate limit, and creates no connection, so the connection calls answer `CONN_NOT_FOUND`

- [sceNpSignalingDeactivateConnection](../../../core/libs/prx/libSceNpSignaling/Export.cpp) (libSceNpSignaling) - not measured on a PS5: it has the same checks as `sceNpSignalingTerminateConnection` (not initialized, unknown context, then `CONN_NOT_FOUND`), after shadPS4's [call](https://github.com/shadps4-emu/shadPS4/blob/12a4564543ccec527f9723676d2180580d1d7a94/src/core/libraries/np/np_signaling/np_signaling.cpp#L378-L400). No connection is ever created, so none can be deactivated.

- [Offline query subset](../../../core/libs/prx/libSceNpSignaling/Export.cpp) (libSceNpSignaling) - the eight implemented query exports are not measured on a PS5: signatures and checks follow [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/d1d0b58cf94e5bfb6a2be406e140dcf1e314853f/src/core/libraries/np/np_signaling/np_signaling.cpp#L172-L819) with no connection or request. `CreateContextA` and `ActivateConnectionA` return `SCE_NP_ERROR_SIGNED_OUT` under the existing libSceNpManager offline model. Console error mapping and validation order remain unverified.

- [Unsettled connection and peer queries](../../../core/libs/prx/libSceNpSignaling/Export.cpp) (libSceNpSignaling) - `sceNpSignalingGetConnectionStatus`, `sceNpSignalingGetConnectionStatistics`, `sceNpSignalingGetConnectionFromPeerAddressA` and `sceNpSignalingGetPeerNetInfoA` have no established console contract for the offline case. Their typed declarations follow the pinned shadPS4 source, but the inactive result, zero counts, context-kind error and validation order are unverified. All four throw through `NotImplemented_nid_no_patch` before examining arguments or state and leave outputs untouched. Closes when: the return values, output writes and check order are established from the firmware module or console measurements.

### Additional callable export inventory

- [Additional callable exports](../../../core/libs/prx/libSceNpSignaling/Export.cpp) - 3 entries have import-resolution declarations only. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. No runtime support, implemented-function increase or title unblock is claimed.

| Name | NID |
| --- | --- |
| `sceNpSignalingGetLocalNetInfo` | `U8AQMlOFBc8` |
| `sceNpSignalingGetMemoryInfo` | `tOpqyDyMje4` |
| `sceNpSignalingGetPeerNetInfo` | `zFgFHId7vAE` |
