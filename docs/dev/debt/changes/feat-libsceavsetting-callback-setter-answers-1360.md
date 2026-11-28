# libSceAvSetting

## change [sceAvControlGetCurrentOutputMode, sceAvControlGetCurrentOutputModeWithSync]

- [sceAvControlGetCurrentOutputMode, sceAvControlGetCurrentOutputModeWithSync](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); measured on a PS5 Pro payload process (13.60): both reject a non-null fourth argument with `0x802A0002` before the state check - shown for writable buffers and, on `sceAvControlGetCurrentOutputMode`, for the fourth-argument values 7 and 1 (tw75d), none dereferenced - while a null fourth argument answers `0x802A001A`: first arguments null (tw75), 7 and 8 (tw76), and buffers pass on both exports; second and third arguments pass as writable buffers (and on `sceAvControlGetCurrentOutputMode` also as the values 7 and 1); no call ever wrote. The "AvCtrl is not initialized" debug print is not reproduced; post-initialization answers are unknown.

## change [sceAvControlSetColorEffect, sceAvControlSetDefaultAudioOutMode]

- [sceAvControlSetColorEffect, sceAvControlSetDefaultAudioOutMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); measured on a PS5 Pro payload process (13.60): `sceAvControlSetColorEffect` accepts 0 and 1 with `0x802A001A` and rejects 2 and 3 with `0x802A0002`, implemented with that range (values above 3 not swept); `sceAvControlSetDefaultAudioOutMode` answers 0 for a null first argument but the session ended when called with 1, so it still throws and its argument shape is unverified.

## change [sceAvSettingAddCallbacksForLnc, sceAvSettingAddCallbacksForSocialScreen]

- [sceAvSettingAddCallbacksForLnc, sceAvSettingAddCallbacksForSocialScreen](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); measured on a PS5 Pro payload process (13.60): both answer `0x802A0001` for four null and four writable-buffer arguments without writing, so they are declared without parameters and return that answer. Whether registered callbacks change the result is unknown.

## change [sceAvSettingChangeOutputMode, sceAvSettingChangeOutputMode2,

- [sceAvSettingChangeOutputMode, sceAvSettingChangeOutputMode2, sceAvSettingChangeOutputMode3, sceAvSettingChangeOutputModeForDiag](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); measured on a PS5 Pro payload process (13.60): all four answer `0x802A001A` for first arguments 0 to 3 and for four writable buffers without writing, so they are declared without parameters and return that answer; the "AvCtrl is not initialized" debug print is not reproduced.

## change [sceAvSettingDebugSetDiagState, sceAvSettingDebugSetHdmiMonitorInfo,

- [sceAvSettingDebugSetDiagState, sceAvSettingDebugSetHdmiMonitorInfo, sceAvSettingDriverChangeConnectionStatus, sceAvSettingDriverUpdateStatus](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); measured on a PS5 Pro payload process (13.60): `sceAvSettingDriverUpdateStatus` answers `0x802A0002` for four nulls, `0x802A0011` for four buffers, without writing; mixed shapes show the first argument decides, implemented as a checked first pointer plus that constant. `sceAvSettingDebugSetDiagState` answers `0x802A0002` for null and ended the session for 1; `sceAvSettingDebugSetHdmiMonitorInfo` answered `0x802A0002` with buffers; both still throw. `sceAvSettingDriverChangeConnectionStatus` answers `0x802A00FF` with four nulls, `0x802A0011` with buffers; the deciding argument is unknown; throws.

## change [sceAvSettingInit, sceAvSettingIsSupportedAudioOutModeByHdmiMonitorInfo,

- [sceAvSettingInit, sceAvSettingIsSupportedAudioOutModeByHdmiMonitorInfo, sceAvSettingIsSupportedHdcpVersionByHdmiMonitorInfo, sceAvSettingIsSupportedVideoOutModeByHdmiMonitorInfo, sceAvSettingIsSuspendedProcessOutputModeAvailable](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); measured on a PS5 Pro payload process (13.60): `sceAvSettingIsSuspendedProcessOutputModeAvailable` answers `0x802A00FF` for all shapes, implemented as that constant; `sceAvSettingIsSupportedVideoOutModeByHdmiMonitorInfo` checks its first pointer (null `0x802A0002`, buffer `0x802A001A`, no write), implemented in that order; the AudioOut and Hdcp twins answer `0x802A0002` for null and filled buffers - content validated against an unknown layout, both throw. `sceAvSettingInit` answered 0 for four nulls; post-initialization is unmeasured, so it throws.

## change [sceAvSettingRemoveCallbacks, sceAvSettingRemoveProcessOutputMode]

- [sceAvSettingRemoveCallbacks, sceAvSettingRemoveProcessOutputMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); measured on a PS5 Pro payload process (13.60): `sceAvSettingRemoveCallbacks` answers `0x802A0011` and `sceAvSettingRemoveProcessOutputMode` answers `0x802A00FF` for four null and four writable-buffer arguments alike without writing, so both are declared without parameters and return those constants.

## change [sceAvSettingSetHdmiGamutMetadata_]

- [sceAvSettingSetHdmiGamutMetadata_](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); measured on a PS5 Pro payload process (13.60): answers `0x802A001A` for first arguments 0 to 3 without writing, declared without parameters and returns that answer; the "AvCtrl is not initialized" debug print is not reproduced.

## change [sceAvControlSetLayoutModeForDiag]

- [sceAvControlSetLayoutModeForDiag](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): accepts 0, 1 and 2 with `0x802A001A` and rejects 3 with `0x802A0002`, implemented with that range; higher values were not swept.

## change [sceAvControlSetMonitorFakeMode]

- [sceAvControlSetMonitorFakeMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers 0 for a null or zero first argument before initialization, `0x802A001A` for 1 and 2 and `0x802A0002` for 3, implemented with that table; values above 3 not swept; the meaning of the success at 0 is unknown.

## change [sceAvControlRemoveProcessOutputMode]

- [sceAvControlRemoveProcessOutputMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers `0x802A00FF` for four null and four writable-buffer arguments without writing; declared without parameters and returns that constant.

## change [sceAvControlUpdateDriverStatus]

- [sceAvControlUpdateDriverStatus](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers `0x802A0002` for four null arguments and `0x802A0011` for four writable buffers without writing; mixed shapes show the first argument decides it - null first with three writable buffers still answers `0x802A0002`, a writable first with three nulls answers `0x802A0011` - implemented as a checked first pointer followed by that constant.

## change [sceAvControlAddCallbacksForLnc]

- [sceAvControlAddCallbacksForLnc](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers `0x802A0001` for four null and four writable-buffer arguments without writing; declared without parameters and returns that constant.

## change [sceAvControlAddCallbacksForResourceArbitrator]

- [sceAvControlAddCallbacksForResourceArbitrator](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers `0x802A0001` for four null arguments and `0x802A00FF` for writable buffers; the first pointer is checked, what an accepted callback block answers is unknown and it still throws for a non-null pointer.

## change [sceAvControlRemoveCallbacks]

- [sceAvControlRemoveCallbacks](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers `0x802A0011` for four null and four writable-buffer arguments without writing; declared without parameters and returns that constant.

## change [sceAvControlWaitCallback]

- [sceAvControlWaitCallback](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers `0x802A0011` immediately for four null arguments - it does not block without registered callbacks; declared without parameters and returns that constant.

## change [sceAvControlSetCheckoutputMode]

- [sceAvControlSetCheckoutputMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers `0x802A001A` for first arguments 0 to 3 without writing; declared without parameters and returns that answer, the "AvCtrl is not initialized" debug print not reproduced.

## change [sceAvControlSetSystemVideoMode]

- [sceAvControlSetSystemVideoMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): accepts 0 and 1 with `0x802A001A` and rejects 2 and 3 with `0x802A0002`, implemented with that range; values above 3 not swept.

## change [sceAvControlDebugAddCallbacks]

- [sceAvControlDebugAddCallbacks](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers `0x802A0001` for four null and four writable-buffer arguments without writing; declared without parameters and returns that constant.
