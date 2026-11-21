# libSceAgcDriver

## remove [sceAgcDriverIsCoredumpValid (`+NIAY3DWpOw`)

## remove [sceAgcDriverSdmaCopyLinearBlocking (`bQ+En9GY3PM`)

## remove [sceAgdSetApplicationGpuDebug (`4JOwdjYqMpU`)

## add Unknown function info

- [sceAgcDriverIsCoredumpValid, sceAgdGetApplicationGpuDebug, sceAgdGetSystemGpuDebug, sceAgdSetApplicationGpuDebug, sceAgcDriverRegisterCaptureInterface, sceAgcDriverDeregisterCaptureInterface, sceAgcDriverIsSuspendPointInFlightDirect, sceAgcDriverReleaseRazorACQ, sceAgcDriverSetTFRingDirect, sceAgcDriverSdmaCopyLinearBlocking](../../../core/libs/prx/libSceAgcDriver/Unimplemented.cpp) (libSceAgcDriver) - a title process may get other results: these return what a payload process got on a PS5 Pro at firmware 13.60 ([#24253](https://github.com/boykopovar/AnyPS5/pull/24253#issuecomment-7039458737)), where the last six returned `0x8A6D0001` after the module logged insufficient permission. The first three return 0 and `sceAgdSetApplicationGpuDebug` returns `0x8A6C0047`. Their signatures are unknown, so they stay parameterless and read no argument. Closes when: measured in a title process. Labels: console
