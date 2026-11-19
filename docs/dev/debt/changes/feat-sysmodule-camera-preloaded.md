# libSceSysmodule

## remove [sceSysmoduleIsCalledFromSysModule, sceSysmoduleIsCameraPreloaded, sceSysmoduleMapLibcForLibkernel, sceSysmodulePreloadModuleForLibkernel and sceSysmoduleUnloadModuleByNameInternal]

## add Unknown function info

- [sceSysmoduleMapLibcForLibkernel and sceSysmodulePreloadModuleForLibkernel](../../../core/libs/prx/libSceSysmodule/Export.cpp) (libSceSysmodule) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSysmodule.c). The exports had no credible prototypes and are declared without parameters solely to resolve known imports; they make no ABI claim.

## add Unknown function info

- [sceSysmoduleIsCameraPreloaded and sceSysmoduleUnloadModuleByNameInternal](../../../core/libs/prx/libSceSysmodule/Export.cpp) (libSceSysmodule) - follow firmware 13.42 ([reading](https://github.com/boykopovar/AnyPS5/issues/21866#issuecomment-6942648672), disassembly only, not run on a console). `IsCameraPreloaded(void)` returns 0 in a PS5 process; its PS4-process path wasn't read, so it throws when `sceKernelIsPs4Process` reports one. `UnloadModuleByNameInternal(handle, args, argp, flags, result)` is a tail jump to `sceKernelStopUnloadModule(handle, args, argp, flags, NULL, result)`, with no checks of its own and no name taken despite its name
