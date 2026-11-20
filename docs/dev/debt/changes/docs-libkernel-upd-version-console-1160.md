# libkernel

## change [sysKernelGetUpdVersion, sysKernelGetManufacturingMode, sysKernelGetIntdevModeForInternalLibc, sysKernelGetIntdevModeForRcmgr]

- [sysKernelGetUpdVersion, sysKernelGetManufacturingMode, sysKernelGetIntdevModeForInternalLibc, sysKernelGetIntdevModeForRcmgr](../../../core/libs/prx/libkernel/System/src/SystemConfiguration.cpp) (libkernel) - follow firmware 13.42 disassembly measured by BunnyFl, querying sysctlbyname: `sysKernelGetUpdVersion` checks null (`SCE_KERNEL_ERROR_EINVAL`) and queries `machdep.upd_version`; `sysKernelGetManufacturingMode` queries `machdep.curr_manumode`; `sysKernelGetIntdevModeForInternalLibc` and `sysKernelGetIntdevModeForRcmgr` query `machdep.rcmgr_intdev_for_internal_libc` and `machdep.rcmgr_intdev`. On AnyPS5, these sysctls return 0 as assumed defaults and reject writes with `EPERM`; on a firmware 11.60 payload process `machdep.upd_version` is not 0 against 0 here (value not recorded), the other three are 0 as here, each getter equals its sysctl, and a null output to `sysKernelGetUpdVersion` returns `SCE_KERNEL_ERROR_EINVAL`, `errno` untouched; write policy and titles are unmeasured.

# libkernel_sys

## change [sceKernelGetUpdVersion]

- [sceKernelGetUpdVersion](../../../core/libs/prx/libkernel_sys/Export.Discovery.cpp) (libkernel_sys) - runs the same code as `sysKernelGetUpdVersion` on firmware 13.42 ([disassembly](https://github.com/boykopovar/AnyPS5/pull/10671#issuecomment-6398286973)), so it forwards there: a null output is refused with `SCE_KERNEL_ERROR_EINVAL`, and otherwise the 4 bytes come from `machdep.upd_version`, whose value on a PS5 payload process (firmware 11.60) is not 0, where AnyPS5 returns 0 (see libkernel's entry); `sceKernelGetUpdVersion` itself was not called there.
