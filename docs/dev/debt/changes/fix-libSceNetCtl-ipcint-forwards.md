# libSceNetCtl

## remove [sceNetCtlGetState2IpcInt]

## change sceNetCtlGetStateIpcInt, sceNetCtlGetStateV6IpcInt

- [sceNetCtlGetStateIpcInt, sceNetCtlGetStateV6IpcInt, sceNetCtlGetState2IpcInt, sceNetCtlGetResultIpcInt, sceNetCtlGetResultV6IpcInt, sceNetCtlGetInfoIpcInt, sceNetCtlGetInfoV6IpcInt, sceNetCtlRegisterCallbackIpcInt, sceNetCtlUnregisterCallbackIpcInt, sceNetCtlRegisterCallbackV6IpcInt and sceNetCtlUnregisterCallbackV6IpcInt](../../../core/libs/prx/libSceNetCtl/Export.cpp) (libSceNetCtl) - answer as their public twins, whose recovered argument registers they share: the getters return and write what `sceNetCtlGetState`, `GetStateV6`, `GetResult`, `GetResultV6`, `GetInfo` and `GetInfoV6` do, throws included, and the callback forms use the IPv4 and IPv6 tables, as no IpcInt check is exported to deliver a table of their own. `sceNetCtlGetState2IpcInt` takes the `int*` write of `sceNetCtlGetState`; which second state it reports beyond `sceNetCtlGetStateIpcInt` is in no source, so it answers that state's value on the matching call shape alone. This rests on the matching call shapes, not on a measurement; whether the internal client answers a null pointer or a bad code before its service does is unknown
