# libSceNet

## change [sceNetAllocateAllRouteInfo6, sceNetDbgInit, sceNetDumpSetFilterParam, sceNetGetAppMemInfo, sceNetGetIfListEx, sceNetGetInterfaceStats, sceNetGetSystemMemInfo]

- [sceNetAllocateAllRouteInfo6, sceNetDbgInit, sceNetDumpSetFilterParam, sceNetGetAppMemInfo, sceNetGetIfListEx, sceNetGetSystemMemInfo](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - absent from the PS4 firmware 10.71 and 11.00 libSceNet exports, so there is no module code to port; they throw.

## add Functional

- [sceNetGetInterfaceStats](../../../core/libs/prx/libSceNet/Export.cpp) (libSceNet) - not measured on a console: the counters are the host's, and whether it needs `sceNetInit` or what it returns while not connected is unverified. Nonzero `flags` and a null `stats` throw; an unwritable `stats` returns `EFAULT` through a checked guest write, keeping the caller's errno. The 64-byte layout (sent bytes, received bytes, 48 reserved bytes) is [SharpProspero](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Interop/Net/Socket.cs#L123-L132)'s, which gives no source and describes one active interface. The counters are [sceNetCtlGetIfStat](../../../core/libs/prx/libSceNetCtl/Export.cpp)'s host totals over up non-loopback interfaces, 0 when none is up.
