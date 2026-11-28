# libSceSysmodule

## remove [sceSysmoduleIsCalledFromSysModule, sceSysmoduleIsCameraPreloaded, sceSysmoduleMapLibcForLibkernel, sceSysmodulePreloadModuleForLibkernel and sceSysmoduleUnloadModuleByNameInternal]

## add Unknown function info

- [sceSysmoduleIsCameraPreloaded and sceSysmoduleUnloadModuleByNameInternal](../../../core/libs/prx/libSceSysmodule/Export.cpp) (libSceSysmodule) - follow firmware 13.42 ([reading](https://github.com/boykopovar/AnyPS5/issues/21866#issuecomment-6942648672), disassembly only, not run on a console). `IsCameraPreloaded(void)` returns 0 in a PS5 process; its PS4-process path wasn't read, so it throws when `sceKernelIsPs4Process` reports one. `UnloadModuleByNameInternal(handle, args, argp, flags, result)` is a tail jump to `sceKernelStopUnloadModule(handle, args, argp, flags, NULL, result)`, with no checks of its own and no name taken despite its name
