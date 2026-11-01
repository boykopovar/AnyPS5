# libSceHidControl technical debt

### Unknown function info

- [sceHidControlConnectHandle, sceHidControlConnectPort, sceHidControlDisconnectDevice, sceHidControlDisconnectHandle, sceHidControlDisconnectPort](../../../core/libs/prx/libSceHidControl/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceHidControl.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceHidControlGetAuthInformation, sceHidControlGetBatteryState, sceHidControlGetDeviceId, sceHidControlGetDeviceInfo, sceHidControlGetDeviceName](../../../core/libs/prx/libSceHidControl/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceHidControl.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceHidControlGetJediExtensionUnitInfo, sceHidControlGetJediState, sceHidControlGetSparkState, sceHidControlGetVersionInfo, sceHidControlInit](../../../core/libs/prx/libSceHidControl/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceHidControl.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceHidControlSetApplicationFocus, sceHidControlSetHandleState, sceHidControlSetJediAudioOutPath, sceHidControlSetJediVolumeGain, sceHidControlSetProcessFocus](../../../core/libs/prx/libSceHidControl/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceHidControl.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceHidControlSetRevokeList, sceHidControlSetRevokeList2, sceHidControlSetUserColor](../../../core/libs/prx/libSceHidControl/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceHidControl.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.
- [sceHidControlSetRevokeList](../../../core/libs/prx/libSceHidControl/Export.cpp) - not exported by any firmware 13.42 module, so on 13.42 an import of these NIDs resolves to nothing in the firmware; the 3.20 declarations remain for titles built against older firmware. Not measured here.

- [sceHidControlEnableMicBeamForming](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetAllVibrationMute](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetVibrationTriggerEffectWeakForce](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlIsForceUpdateNeeded](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetRemoteMergeProcess](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetCollaboInputSettingOverrideProcess](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetForceAdvancedVibrationMode](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlPswUpdateIsUpdateNeeded](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlPswUpdateIsUpdateNeededForMultiDevice](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlPswUpdateIsReady](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlPswUpdateStart](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlPswUpdateGetStatus](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlPswUpdateSendData](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlPswUpdateSendAppendixData](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlPswUpdateGetReconnectStatus](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlPswUpdateGetDeviceFirmwareUpdateTelemetryData](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswState](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswBatteryState](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetPswDeviceName](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswDeviceName](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetPswConsoleMute](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetPswSpeakerVolume](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetPswSidetoneLevel](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswSidetoneLevel](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetPswEqPresetSelection](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswEqPresetSelection](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetPswEqPresetName](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswEqPresetName](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetPswEqBandValues](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswEqBandValues](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswAllEqBandValues](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetPswResetSettings](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetPswNetworkLocale](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswNetworkLocale](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswDeviceNameForNearby](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlConnectPswDeviceForNearby](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswDeviceInfo](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlSetPswCommand](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswCommand](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature
- [sceHidControlGetPswDeviceIdFromIdxAdd](../../../core/libs/prx/libSceHidControl/Export.cpp) (libSceHidControl) - unknown signature

### Additional callable export inventory

- [Additional exports](../../../core/libs/prx/libSceHidControl/Export.cpp) - 19 import-resolution declarations (5 named candidates and 14 unresolved NIDs). All entries call `NotImplemented_nid_no_patch`. Parameterless `int` declarations do not establish parameter or return ABI; no runtime implementation or title unblock is claimed.

#### Membership and identity evidence

The inspected `PT_SCE_MODULEPARAM` record contains version words `0x13590001` at offset `0x10` and `0x13600007` at offset `0x14`. The 13.60 label refers to the latter SDK-formatted word, not independently authenticated retail firmware or an introduction date. The [reproducible static audit](https://github.com/boykopovar/AnyPS5/pull/12367#issuecomment-6470603298) records the extraction/filtering procedure and the observed NID-set digest. Each row below was read as a defined `STT_FUNC` entry; RVA and size are symbol-table values, not runtime addresses or function bytes.

Unique function NIDs in the inspected module: **81**. SHA-256 of sorted unique NIDs (one ASCII NID plus LF per row): `287013aa78cc6ad0042a719d9dab918c23d24bd5165a99dd1591e5152de3927d`. All 19 proposed NIDs are members of that set.

Candidate names marked **aerolib** occur in the [repository-pinned table](https://github.com/zecoxao/sce_symbols/blob/2883963a0a514ba6e77407a08ef4730a65e88254/aerolib.csv). Names marked **public candidate** occur in the [public ps5rs identity database at commit `2304be5131221bae0dac24b9767cabffda33f747`](https://github.com/claimore22/ps5rs/blob/2304be5131221bae0dac24b9767cabffda33f747/data/nids.csv). Every named candidate was recomputed with the repository NID algorithm and matched to a function NID in the inspected table; no name was assigned by ordinal or adjacent address. Those databases support spelling, not firmware attribution, prototypes or behavior. Unresolved names remain explicit NID aliases.

| Candidate / unresolved | NID | Name evidence | ELF RVA | Symbol bytes |
| --- | --- | --- | --- | ---: |
| `sceHidControlGetControllerSerialNumberInformation` | `0Jn+BBzGKH4` | public candidate | `0x2520` | 165 |
| `sceHidControlGetPswNearbyDeviceInfo` | `XmwsppH3YDI` | public candidate | `0xe00` | 110 |
| `sceHidControlPswUpdateInit` | `05Zuhe4dnWA` | public candidate | `0x1fb0` | 94 |
| `sceHidControlPswUpdateTerm` | `H6ctF1YKmCU` | public candidate | `0x23a0` | 94 |
| `sceHidControlSetPswControllerCommunicationMethod` | `ieH2et01RXU` | public candidate | `0x25d0` | 108 |
| `Unknown` | `00qes5NswlQ` | unresolved | `0x1f00` | 166 |
| `Unknown` | `8fIm7UR4VDc` | unresolved | `0x17e0` | 106 |
| `Unknown` | `AjBZjowWaFI` | unresolved | `0x1a80` | 107 |
| `Unknown` | `C05jP9w4PCc` | unresolved | `0xa40` | 90 |
| `Unknown` | `DUGI7AXFtV0` | unresolved | `0x24b0` | 100 |
| `Unknown` | `HZJRMD3BeKk` | unresolved | `0x540` | 107 |
| `Unknown` | `OFyJjudLKlA` | unresolved | `0x1760` | 118 |
| `Unknown` | `TKax3WNHtEg` | unresolved | `0x2290` | 98 |
| `Unknown` | `WikCB91EU0w` | unresolved | `0xd90` | 110 |
| `Unknown` | `i5ALHbvjRi8` | unresolved | `0x15a0` | 103 |
| `Unknown` | `iVF5VrBHJzk` | unresolved | `0x900` | 101 |
| `Unknown` | `tcDE01gUoN0` | unresolved | `0x8a0` | 96 |
| `Unknown` | `w2MEBm2Tvm8` | unresolved | `0xb70` | 90 |
| `Unknown` | `yJJq5Arw8EI` | unresolved | `0xaa0` | 95 |
