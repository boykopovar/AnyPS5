# libSceAvSetting

## change [sceAvControlSetColorMode]

- [sceAvControlSetColorMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): the first argument is an enum accepting 0 and 2 to 5 with `0x802A001A` and rejecting 1 and 6 to 9 with `0x802A0002`; buffer addresses are rejected too, consistent with reading only the low 32 bits. Implemented as that domain; higher values were not swept and the meaning of the reserved 1 is unknown.

## change [sceAvSettingIsSuspendedProcessOutputModeAvailable2, sceAvSettingIsVrSupportedByHdmiMonitorInfo2,

- [sceAvSettingIsSuspendedProcessOutputModeAvailable2, sceAvSettingIsVrSupportedByHdmiMonitorInfo2, sceAvSettingNotifyResponseForSocialScreen, sceAvSettingNotifyUmdEvent](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); measured on a PS5 Pro payload process (13.60): `sceAvSettingIsSuspendedProcessOutputModeAvailable2` answers `0x802A00FF` always and stores an u32 of live system state unchecked through the third (0 with four buffers, 7 with a null first); a null third argument faults, a null first does not; hence it throws. `sceAvSettingIsVrSupportedByHdmiMonitorInfo2` answers `0x802A001A` for buffers without writing and faults on nulls; `sceAvSettingNotifyResponseForSocialScreen` answers `0x802A00FF` for nulls and `0x802A0002` for buffers, no argument distinguished; `sceAvSettingNotifyUmdEvent` answers `0x8a4a3635` for buffers without writing and faults on nulls, dereferencing an unknown argument. All three still throw.

## change [sceAvControlSetDefaultVideoOutMode]

- [sceAvControlSetDefaultVideoOutMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): four null arguments answer 0; four writable buffers make it write `ff 00 00 00 ff 00 00 00` through the first argument and return `0x2e2c94f0`, which looks like an internal address rather than a code; an integer first argument of 1 ends the session. It fills a default video-out mode structure whose layout is unknown, so it still throws. The `sceAvSetting` spelling of this name is not an export on 13.60 (`dlsym` answers null) and does not exist here either.

## change [sceAvControlNotifyUmdEvent]

- [sceAvControlNotifyUmdEvent](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers `0x8a4a3635` for four writable buffers without writing and ends the session for four null arguments, like its `sceAvSetting` twin; it still throws because which argument is dereferenced is unknown.

## change [sceAvSettingSetDisplayBlackLevel, sceAvSettingTerm]

- [sceAvSettingSetDisplayBlackLevel, sceAvSettingTerm](../../../core/libs/prx/libSceAvSetting/Export.cpp) - on firmware 13.42 both return `0x802A00FF` and do nothing else, measured in an emulator twice with null, small and all-ones arguments ([BunnyFl on #9449](https://github.com/boykopovar/AnyPS5/pull/9449#issuecomment-6401358322)); they return that code here. Confirmed on a PS5 Pro payload process (13.60): both answer `0x802A00FF` for four null arguments, four writable buffers and small integers without writing, so the no-parameter declaration covers every measured shape.

## change [sceAvSettingControlHdcpEncryption, sceAvSettingDebugSetProcessAttribute,

- [sceAvSettingControlHdcpEncryption, sceAvSettingDebugSetProcessAttribute, sceAvSettingEnterAudioMuteForShutdown, sceAvSettingNotifyAudioOutMode, sceAvSettingNotifyVideoOutMode, sceAvSettingSet2dVrMode, sceAvSettingSetAvOutputMode, sceAvSettingSetDispclk and sceAvSettingSetHdcpStatus](../../../core/libs/prx/libSceAvSetting/Export.cpp) - pre-initialization answer only, from firmware 13.42 disassembly, not console-measured: the state word at `0x1e10` of the object at `0x20420` is set to 2 only by `sceAvControlInit`, `sceAvControlInit2` and `sceAvSettingInit`, which still throw here, so these return `0x802A001A`. Each forwards to an sceAvControl method with the same argument check (`0x802A0002`); trailing arguments are not declared and names are not from a header. The `sceAvSettingDebugSetProcessAttribute` and `sceAvSettingNotifyAudioOutMode` checks were confirmed on a PS5 Pro payload (13.60): nulls get the initialization answer, buffers are rejected `0x802A0002` without writing.
