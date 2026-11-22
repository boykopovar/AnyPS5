# libkernel

## remove [sceKernelAllocateToolMemory, sceKernelAvailableToolMemorySize, sceKernelMapToolMemory, sceKernelReleaseToolMemory, sceKernelGetToolMemoryRange and sceKernelSetProcessProperty]

## add Functional

- [sceKernelGetToolMemoryRange, sceKernelAllocateToolMemory, sceKernelAvailableToolMemorySize, sceKernelMapToolMemory and sceKernelReleaseToolMemory](../../../core/libs/prx/libkernel/SysExports.cpp) (libkernel and libkernel_sys) - the tool-memory tier of the direct-memory allocator, from firmware 4.03's `libkernel.sprx`: the range query is dmem ioctl `0xC018801D`; allocate is `0xC0288001` with the selector where direct allocation writes zero; the rest carry the direct-memory parameter lists. Only partitions `0x13`/`0x14` — PPSA31544's call-site values — share the one tracked pool; other ids throw, their console bounds unmeasured. Firmware's map also ORs `0x2400` and defaults a null base to `0x50000000000`, neither reproduced. `sceKernelQueryToolMemory` and `sceKernelGetDefaultToolMemorySize` stay unimplemented
- [sceKernelSetProcessProperty](../../../core/libs/prx/libkernel/SysExports.cpp) (libkernel and libkernel_sys) - verified against firmware 4.03's `libkernel.sprx`: `(const void* name, int64_t, int64_t, int64_t, int64_t)` copies the 32-byte record `name` points at plus the four values into a syscall record headed by `0x4ffffffff`; a null `name` returns `SCE_KERNEL_ERROR_EINVAL`. The shared export calls `KernelSetProcessProperty_nid_no_patch` in [Process.cpp](../../../core/libs/prx/libkernel/System/src/Process.cpp), which stores `{name32, value[4]}` in one process map and returns `SCE_KERNEL_ERROR_EFAULT` for an unreadable name. What the property controls, whether a duplicate key replaces, and the `0x5ffffffff`-record sibling `sceKernelSetProcessPropertyString` are unmeasured; the `sceDebugGetProcessPropertyForCoredump` reader remains a stub

# libkernel_sys

## remove `sceKernelGetToolMemoryRange`
