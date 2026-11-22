# libSceAvSetting

## change [sceAvControlIsSuspendedProcessOutputModeAvailable, sceAvControlIsVideoOutModeSupportedByMonitorInfo]

- [sceAvControlIsSuspendedProcessOutputModeAvailable, sceAvControlIsVideoOutModeSupportedByMonitorInfo](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - names and NIDs match the [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceAvSetting.c); measured on a PS5 Pro payload process (13.60): `sceAvControlIsSuspendedProcessOutputModeAvailable` answers `0x802A00FF` without reading arguments (four null and four writable-buffer arguments alike), like the NID-bound export that already returns it here; `sceAvControlIsVideoOutModeSupportedByMonitorInfo` answers `0x802A001A` for both call shapes without writing to any buffer, so both are declared without parameters and return those answers. The module's "AvCtrl is not initialized" debug print is not reproduced; what an initialized console answers is unknown.

## change [sceAvControlCheckCallback]

- [sceAvControlCheckCallback](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers `0x802A0011` without reading arguments (four null and four writable-buffer arguments alike, no debug print); declared without parameters and returns that answer. Whether an initialized console with registered callbacks answers differently is unknown.

## change [sceAvControlCheckMonitorInfoCapability]

- [sceAvControlCheckMonitorInfoCapability](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): answers 0 with four null arguments but `0x802A0002` when given writable buffers filled with `0x5a` before reaching the initialization check, so some argument is validated as a monitor-info structure whose layout is unknown and null means "skip"; which argument carries it is not distinguished. Still throws for non-null arguments.

## change [sceAvControlCheckVideoOutMode]

- [sceAvControlCheckVideoOutMode](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): with four null arguments it reaches the initialization check and answers `0x802A001A`, but writable buffers filled with `0x5a` are rejected with `0x802A0002` before it, so a non-null monitor-info argument is validated against a layout that is unknown; which argument carries it is not distinguished. Still throws for non-null arguments.

## change [sceAvControlIsVideoOutModeSupportedByMonitorInfoWithOptions]

- [sceAvControlIsVideoOutModeSupportedByMonitorInfoWithOptions](../../../core/libs/prx/libSceAvSetting/Export.cpp) (libSceAvSetting) - measured on a PS5 Pro payload process (13.60): with four null arguments it answers `0x802A001A` after the "AvCtrl is not initialized" state check, but writable buffers filled with `0x5a` are rejected with `0x802A0002` before it - unlike its parameter-less twin; a non-null monitor-info argument is validated against an unknown layout and which argument carries it is not distinguished. Still throws for non-null arguments.
