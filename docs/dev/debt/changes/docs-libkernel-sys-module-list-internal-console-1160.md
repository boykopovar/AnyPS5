# libkernel_sys

## change [sceKernelGetModuleInfoInternal, sceKernelGetModuleListInternal and sceKernelLoadStartModuleForSysmodule]

- [sceKernelGetModuleInfoInternal and sceKernelLoadStartModuleForSysmodule](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) answer for every loaded module, system modules included. `sceKernelGetModuleInfoInternal` fills the 0x1A8-byte `SceKernelModuleInfoEx` of a module handle the way `sceKernelGetModuleInfoFromAddr` does for an address, with [shadPS4's checks](https://github.com/shadps4-emu/shadPS4/blob/a808e76/src/core/libraries/kernel/process.cpp#L243-L258) in their order: `EFAULT` for a null pointer, `EINVAL` when `st_size` is not 0x1A8, `ESRCH` for an unknown handle. A PS5 (firmware 11.60, payload process; a title process may differ) checked `sceKernelGetModuleInfoInternal` in that order (run `2026-11-18T050545Z_N2_module_info_internal`). `sceKernelLoadStartModuleForSysmodule` forwards to `sceKernelLoadStartModule`

## add Functional

- [sceKernelGetModuleListInternal](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) returns the same list as `sceKernelGetModuleList`, as [fpPS4](https://github.com/red-prig/fpPS4/blob/04cefd4/kernel/ps4_libkernel.pas#L852-L858) does; fpPS4 marks that as a guess, and a PS5 (firmware 11.60, payload process; a title process may differ) listed 5 modules (`eboot.bin` and four system `.sprx` modules) where `sceKernelGetModuleList` listed 1 (run `2026-11-18T050545Z_N2_module_info_internal`), so the two lists differ there.
