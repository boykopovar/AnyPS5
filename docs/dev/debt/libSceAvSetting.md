# libSceAvSetting technical debt

### Unknown function info

- [sceAvControlChangeOutputLayout, sceAvControlChangeOutputMode, sceAvControlChangeOutputModeForDiag, sceAvControlChangeProcessAttribute, sceAvControlChangeScanInMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvControlGetCurrentDeviceId, sceAvControlGetCurrentOutputMode, sceAvControlGetCurrentOutputModeWithSync, sceAvControlGetDetailHdcpStatus, sceAvControlGetDeviceInfo](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvControlGetHdcpStatus, sceAvControlGetMonitorInfo, sceAvControlGetNativeMonitorInfo, sceAvControlGetPuStatus, sceAvControlGetPuType](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvControlInit, sceAvControlIsAudioOutModeSupportedByMonitorInfo, sceAvControlIsColorimetryHdr, sceAvControlIsDeepColorSupportedByMonitorInfo, sceAvControlIsDeviceConnected](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvControlIsHdrSupportedByMonitorInfo, sceAvControlIsModeGen1Vr, sceAvControlIsModeSystemDefault, sceAvControlIsSuspendedProcessOutputModeAvailable, sceAvControlIsVideoOutModeSupportedByMonitorInfo](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvControlSet2dVrMode, sceAvControlSetAudioMuteForShutdown, sceAvControlSetAvOutputMode, sceAvControlSetColorEffect, sceAvControlSetDefaultAudioOutMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvControlSetDefaultVideoOutMode, sceAvControlSetDispclk, sceAvControlSetGamutMetadata, sceAvControlSetHdcpEncryption, sceAvControlSetHdcpStatus](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvControlSetHdmiGamutMetadata, sceAvControlSetHdrMetadata, sceAvControlSetVideoOutSource, sceAvControlSimulateProcessOutputModeArbitration, sceAvSettingAddCallbacks](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvSettingAddCallbacksForLnc, sceAvSettingAddCallbacksForSocialScreen](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvControlInit2](../../../core/libs/prx/libSceAvSetting/Export.cpp) throws on every call. Its name and NID `Jhcj4HDIw04` are confirmed in the pinned [PS5 13.60 inventory](https://github.com/xdCloudy/PS5-13.60-Function-Stub-Library/blob/bfec40deb4c47a17d7603ee2bc79aa6862c0c56b/data/firmware-13.60/libsceavsetting-620714af3c.jsonl); signature and behavior remain unknown. The parameterless `int` declaration is an import placeholder, not a verified prototype or working implementation.
- [sceAvSettingChangeOutputMode, sceAvSettingChangeOutputMode2, sceAvSettingChangeOutputMode3, sceAvSettingChangeOutputModeForDiag](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvSettingCallbackFuncsInit, sceAvSettingCallbackFuncsInit_, sceAvSettingCallbackFuncsInitForLnc, sceAvSettingCallbackFuncsInitForLnc_](../../../core/libs/prx/libSceAvSetting/Export.cpp) - the layout of the callback block after its size field is unknown. They follow firmware 13.42's libSceAvSetting.sprx, read from its disassembly and run natively on x86-64 (not a console): each takes one pointer and returns nothing; a null pointer is ignored; otherwise `sceAvSettingCallbackFuncsInit` and `sceAvSettingCallbackFuncsInit_` clear bytes 0 to 31 and store 0x20 as a 32-bit value at byte 0, and `sceAvSettingCallbackFuncsInitForLnc` and `sceAvSettingCallbackFuncsInitForLnc_` clear bytes 0 to 15 and store 0x10 there. Later bytes are left unchanged. None of them needs `sceAvSettingInit`.
- [sceAvSettingChangeProcessAttribute, sceAvSettingCheckCallback, sceAvSettingControlHdcpEncryption, sceAvSettingDebugAddCallbacks, sceAvSettingDebugClearDiagCommand](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvSettingDebugSetDiagState, sceAvSettingDebugSetHdmiMonitorInfo, sceAvSettingDebugSetProcessAttribute, sceAvSettingDriverChangeConnectionStatus, sceAvSettingDriverUpdateStatus](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvSettingEnterAudioMuteForShutdown, sceAvSettingGetCurrentDeviceInfo_, sceAvSettingGetCurrentHdmiDeviceId, sceAvSettingGetCurrentOutputMode2_, sceAvSettingGetCurrentOutputMode_](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvSettingGetDetailedHdcpStatus, sceAvSettingGetDeviceInfo, sceAvSettingGetHdcpStatus, sceAvSettingGetHdmiConnectDisconnectNum, sceAvSettingGetHdmiKsvList](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvSettingGetHdmiMonitorInfo, sceAvSettingGetHdmiRawEdid, sceAvSettingGetMorpheusPuStatus, sceAvSettingGetNativeHdmiMonitorInfo, sceAvSettingGetRawHdmiMonitorInfo](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvSettingInit, sceAvSettingIsSupportedAudioOutModeByHdmiMonitorInfo, sceAvSettingIsSupportedHdcpVersionByHdmiMonitorInfo, sceAvSettingIsSupportedVideoOutModeByHdmiMonitorInfo, sceAvSettingIsSuspendedProcessOutputModeAvailable](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvSettingIsSuspendedProcessOutputModeAvailable2, sceAvSettingIsVrSupportedByHdmiMonitorInfo2, sceAvSettingNotifyAudioOutMode, sceAvSettingNotifyResponseForSocialScreen, sceAvSettingNotifyUmdEvent](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvSettingNotifyVideoOutMode, sceAvSettingRemoveCallbacks, sceAvSettingRemoveProcessOutputMode, sceAvSettingSet2dVrMode, sceAvSettingSetAudioOutModeAny_](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvSettingSetAudioOutModeInvalid_, sceAvSettingSetAvOutputMode, sceAvSettingSetDispclk, sceAvSettingSetHdcpMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvSettingSetHdcpStatus, sceAvSettingSetHdmiGamutMetadata_, sceAvSettingSetVideoOutModeAny_, sceAvSettingSetVideoOutModeInvalid_, sceAvSettingSetVideoOutSource](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceAvSettingSetVideoOutputColorEffect, sceAvSettingSimulateProcessOutputModeArbitration_, sceAvSettingWaitCallback](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.

- [sceAvControlGetPuTypeByMonitorInfo](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlIsVideoOutModeSupportedByMonitorInfoWithOptions](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlSetDitherModeForDiag](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlSetLayoutModeForDiag](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlSetMonitorFakeMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlGetCurrentOutputSysMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlGetNativeHdmiMonitorInfo](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
### Additional callable exports

- [Additional callable exports](../../../core/libs/prx/libSceAvSetting/Export.cpp) - 28 entries present in both version-labelled images are import-resolution declarations only. None is listed in the pinned public 3.20 catalogue for this library. Neither label independently authenticates retail firmware provenance. Parameter lists, return types and behavior are unverified; parameterless `int` declarations are placeholders, not verified ABI contracts. Every entry throws through `NotImplemented_nid_no_patch`. Unidentified names use literal NID aliases. No runtime functionality is implemented. Data exports require separate layout and ownership analysis. Identified candidate names match their NIDs in the [public symbol database](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv) or [supplemental catalog](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv).

| Name | NID |
| --- | --- |
| `Unknown` | `+QkxEV+rZws` |
| `Unknown` | `0BKuKlQG8zE` |
| `Unknown` | `0mMgouCMXQ4` |
| `Unknown` | `0vwiTglFTz4` |
| `Unknown` | `7kywfNahIbw` |
| `Unknown` | `A143jTEYkNw` |
| `Unknown` | `DtJ45BO7LtY` |
| `Unknown` | `Dxe4ybR-NaM` |
| `Unknown` | `GYAho0we28c` |
| `Unknown` | `Iv3zoMND6xU` |
| `Unknown` | `JUxonNYB9xk` |
| `Unknown` | `JlZQqZOsTMQ` |
| `Unknown` | `MNZaRdvvdU4` |
| `Unknown` | `V-IcK9eokeE` |
| `Unknown` | `bM9VthphLmg` |
| `Unknown` | `bXmd6MGBaNI` |
| `Unknown` | `cANt7oF1VsA` |
| `Unknown` | `cK+dKi9GYYA` |
| `Unknown` | `ftQASJeHi6M` |
| `Unknown` | `kvYuLJa0CyU` |
| `Unknown` | `m1Wi0Yp38v0` |
| `Unknown` | `mp2s52Yr4pc` |
| `Unknown` | `nWFSNYt07Cw` |
| `Unknown` | `nhvN5B5bvyc` |
| `Unknown` | `osJaW+pc9kw` |
| `Unknown` | `s-Egk+i6sAY` |
| `Unknown` | `tIR02KtiDXw` |
| `Unknown` | `xVRs9kzSP7I` |

### Unknown function info (continued)

- [sceAvControlGetRawMonitorInfo](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlGetKsvList](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlGetRawEdid](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlRemoveProcessOutputMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlUpdateDriverStatus](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlNotifyUmdEvent](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlAddCallbacksForLnc](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlAddCallbacksForResourceArbitrator](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlRemoveCallbacks](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlCheckCallback](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlWaitCallback](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlSetColorMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlCheckMonitorInfoCapability](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlCheckVideoOutMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlSetCheckoutputMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlSetSystemVideoMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvControlDebugAddCallbacks](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - unknown signature
- [sceAvSettingSetDisplayBlackLevel, sceAvSettingTerm](../../../core/libs/prx/libSceAvSetting/Export.cpp) - on firmware 13.42 both return `0x802A00FF` and do nothing else, measured in an emulator twice with null, small and all-ones arguments ([BunnyFl on #9449](https://github.com/boykopovar/AnyPS5/pull/9449#issuecomment-6401358322)); they return that code here. Signatures and behavior on other firmware are unverified.
