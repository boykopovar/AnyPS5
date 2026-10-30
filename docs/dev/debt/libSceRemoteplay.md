# libSceRemoteplay technical debt

### Silent stubs

- [sceRemoteplayGetConnectionStatus](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - reports `REMOTEPLAY_CONNECTION_STATUS_DISCONNECT` (0) for any user ID: on the console the value comes from the session service, and the wrapper retries once through another internal query when that answers `0x80020003`; this tree has no session, so the value and the retry path are unmodelled

### Functional

- [sceRemoteplayInitialize, sceRemoteplayTerminate](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - the state machine follows the firmware 11.00 module (`libSceRemoteplay.sprx`), not checked on 13.x or on a console. `Initialize` (`0x13a0`) and `Terminate` (`0x13d0`) are thin wrappers over one singleton. An absent heap is accepted and the module allocates and aligns a 0x1800-byte heap itself, whose allocation failure is `0x80fc0002`; a present heap must be at least 0x1800 (`0x80fc0001`), and a call while the state is initialized returns `0x80fc0003`. Terminate returns `0x80fc0004` unless the module is initialized and leaves a terminated state that a later initialize accepts, so the state is not a boolean
- [sceRemoteplayInitialize](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - the 0x1800 heap contents (the session object), the 5,000,000-unit timeout and the two virtual calls that initialize performs are unmodelled
- [sceRemoteplayGetConnectionStatus](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - the state checks follow the 11.00 module (`0x1520`), not checked on 13.x or on a console: it returns `0x80fc0004` unless the module is initialized, checks the state before the output pointer and answers `0x80fc0001` for a null output

### Unknown function info

- [libSceRemoteplay additional exports](../../../core/libs/prx/libSceRemoteplay/Export.cpp) - these name/NID pairs are present in the 13.60 export inventory. Parameter lists, return types and behavior are unverified. Parameterless `int` declarations are import-resolution placeholders, not verified ABI prototypes; every entry throws through `NotImplemented_nid_no_patch`. No runtime functionality is implemented.

| Name | NID |
| --- | --- |
| `sceRemoteplayIsRemoteViewering` | `PBhv2eFxDDs` |
| `sceRemoteplayNotifyCanWakeup` | `KHxNaWydXss` |
| `sceRemoteplayRegistCallback` | `pv4ZWTRweyI` |
| `sceRemoteplayUnregistCallback` | `BHUdZh6st9c` |

- [sceRemoteplayApprove](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayClearAllRegistData](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayClearConnectHistory](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayConfirmDeviceRegist](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayDisconnect](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayGeneratePinCode](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayGetApMode](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayGetConnectHistory](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayGetConnectUserId](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayGetMbusDeviceInfo](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayGetOperationStatus](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayGetPrimaryUserId](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayGetRemoteplayStatus](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayGetRpMode](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayImeClose](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayImeFilterResult](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayImeGetEvent](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayImeNotify](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayImeNotifyEventResult](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayImeOpen](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayImeSetCaret](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayImeSetText](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayIsRemoteOskReady](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayIsRemotePlaying](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayNotifyMbusDeviceRegistComplete](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayNotifyNpPushWakeup](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayNotifyPinCodeError](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayNotifySubmitCode](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayNotifyUserDelete](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayPrintAllRegistData](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayProhibit](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayProhibitStreaming](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayServerLock](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplayServerUnLock](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplaySetApMode](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplaySetLogLevel](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplaySetProhibition](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplaySetProhibitionForVsh](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
- [sceRemoteplaySetRpMode](../../../core/libs/prx/libSceRemoteplay/Export.cpp) (libSceRemoteplay) - declared without parameters, signature unknown; name from the PS5 3.20 export inventory (DNNDHH/PS5-3.20_Libs)
