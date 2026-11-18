# libSceAvSetting

## change [sceAvControlChangeOutputLayout, sceAvControlChangeOutputMode, sceAvControlChangeOutputModeForDiag, sceAvControlChangeProcessAttribute, sceAvControlChangeScanInMode]

- [sceAvControlChangeOutputLayout, sceAvControlChangeOutputMode, sceAvControlChangeOutputModeForDiag, sceAvControlChangeScanInMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.

## change [sceAvControlInit, sceAvControlIsAudioOutModeSupportedByMonitorInfo, sceAvControlIsDeviceConnected]

- [sceAvControlInit, sceAvControlIsAudioOutModeSupportedByMonitorInfo](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.

## change [sceAvControlSetHdrMetadata, sceAvControlSetVideoOutSource, sceAvSettingAddCallbacks]

- [sceAvControlSetVideoOutSource, sceAvSettingAddCallbacks](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.

## change [sceAvSettingChangeProcessAttribute, sceAvSettingCheckCallback, sceAvSettingDebugAddCallbacks, sceAvSettingDebugClearDiagCommand]

- [sceAvSettingCheckCallback, sceAvSettingDebugAddCallbacks, sceAvSettingDebugClearDiagCommand](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.

## change [sceAvSettingGetCurrentDeviceInfo_, sceAvSettingGetCurrentHdmiDeviceId, sceAvSettingGetCurrentOutputMode2_, sceAvSettingGetCurrentOutputMode_]

- [sceAvSettingGetCurrentHdmiDeviceId, sceAvSettingGetCurrentOutputMode2_, sceAvSettingGetCurrentOutputMode_](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.

## change [sceAvSettingSetVideoOutputColorEffect, sceAvSettingSimulateProcessOutputModeArbitration_, sceAvSettingWaitCallback]

- [sceAvSettingSetVideoOutputColorEffect, sceAvSettingWaitCallback](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); signatures and behavior are unverified. No-parameter declarations resolve imports and throw through `NotImplemented_nid_no_patch` when called.

## remove `0BKuKlQG8zE`

## remove `MNZaRdvvdU4`

## add Functional

- [sceAvControlIsDeviceConnected, sceAvControlChangeProcessAttribute, sceAvSettingChangeProcessAttribute, sceAvControlSetHdrMetadata, sceAvSettingGetCurrentDeviceInfo_, sceAvSettingSimulateProcessOutputModeArbitration_, 0BKuKlQG8zE and MNZaRdvvdU4](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - only the argument checks and the answer before initialization are implemented, from firmware 13.42 read in disassembly and run under an emulator, not measured on a console: the checks return `0x802A0002` (`0x802A0007` for the metadata type of SetHdrMetadata and `MNZaRdvvdU4`), and past them the state word at `0x1e10` of the object at `0x20420` is not 2 until `sceAvControlInit`, `sceAvControlInit2` or `sceAvSettingInit` runs, which still throw here, so these return `0x802A001A`; the module's `printf` on that path is not reproduced. Only the arguments the checks read are declared; their names are not from a header
