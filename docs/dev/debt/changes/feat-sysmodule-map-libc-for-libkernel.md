# libSceSysmodule

## change [sceSysmoduleIsCalledFromSysModule, sceSysmoduleIsCameraPreloaded, sceSysmoduleMapLibcForLibkernel, sceSysmodulePreloadModuleForLibkernel and sceSysmoduleUnloadModuleByNameInternal]

- [sceSysmoduleIsCalledFromSysModule, sceSysmoduleIsCameraPreloaded, sceSysmodulePreloadModuleForLibkernel and sceSysmoduleUnloadModuleByNameInternal](../../../core/libs/prx/libSceSysmodule/Export.cpp) (libSceSysmodule) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceSysmodule.c). The exports had no credible prototypes and are declared without parameters solely to resolve known imports; they make no ABI claim.

## add Functional

- [sceSysmoduleMapLibcForLibkernel](../../../core/libs/prx/libSceSysmodule/Export.cpp) (libSceSysmodule) - the PS5 preload flag stays clear because `sceSysmodulePreloadModuleForLibkernel` is not implemented, so ordinary calls write -1 and return 0x5d until that export can set the state. The PS4 internal-process exclusion flags are not modelled; the host exposes the title-process path. Module loading uses `sceKernelLoadStartModule`, which starts the module, in place of the firmware's unavailable `__sys_dynlib_load_prx`, and propagates the loader result rather than the syscall wrapper's exact error. An unreadable output pointer throws while preserving errno instead of faulting as the firmware does. Closes when: the preload export and PS4 process flags are implemented, the dynlib load syscall is available, and bad guest memory and loader failures are measured on a PS5.
