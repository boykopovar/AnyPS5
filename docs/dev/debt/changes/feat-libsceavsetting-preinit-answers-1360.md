# libSceAvSetting

## change [sceAvSettingGetDetailedHdcpStatus, sceAvSettingGetDeviceInfo,

- [sceAvSettingGetDetailedHdcpStatus, sceAvSettingGetDeviceInfo, sceAvSettingGetHdcpStatus, sceAvSettingGetHdmiConnectDisconnectNum, sceAvSettingGetHdmiKsvList](../../../core/libs/prx/libSceAvSetting/Export.cpp) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); measured on a PS5 Pro payload process (13.60): all five check an output pointer before initialization, answering `0x802A0002` for null; `sceAvSettingGetHdcpStatus` writes 7 as a 32-bit value into a writable destination and answers `0x802A001A`; the other four answer `0x802A001A` without writing; `sceAvSettingGetDeviceInfo` and `sceAvSettingGetHdmiKsvList` ignore their first argument (pointer or id not distinguished) and check the second, so both take two arguments. `sceAvSettingInit` still throws, so post-initialization answers are unknown.

## change [sceAvControlGetPuTypeByMonitorInfo]

- [sceAvControlGetPuTypeByMonitorInfo](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): a null first argument answers `0x802A0002`; with a writable first argument the session ended before any answer or debug print appeared, so later arguments are not declared and the export still throws for a writable pointer.

## change [sceAvControlGetCurrentOutputSysMode]

- [sceAvControlGetCurrentOutputSysMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers `0x802A001A` when called with four null arguments, so no argument is checked before initialization; declared without parameters and returns that answer, the module's "AvCtrl is not initialized" debug print not reproduced. Whether an initialized console takes arguments this export uses is unknown.

## change [sceAvControlGetNativeHdmiMonitorInfo]

- [sceAvControlGetNativeHdmiMonitorInfo](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): the **second** argument is the checked output pointer - null second answers `0x802A0001` with either first argument, writable second answers `0x802A001A` with the "AvCtrl is not initialized" print and no write - so it is declared `(std::uint64_t, void* info)` like its siblings; the first argument is ignored.
