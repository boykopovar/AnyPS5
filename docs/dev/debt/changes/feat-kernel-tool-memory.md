# libkernel

## change [sceKernelAllocateToolMemory, sceKernelAvailableToolMemorySize, sceKernelMapToolMemory, sceKernelReleaseToolMemory, sceKernelGetToolMemoryRange and sceKernelSetProcessProperty]

- [sceKernelSetProcessProperty](../../../core/libs/prx/libkernel/SysExports.cpp) (libkernel and libkernel_sys) - a throwing import-resolution declaration; firmware 4.03 copies a 32-byte record the caller points at plus four further 64-bit values into a syscall record headed by the constant `0x4ffffffff`, and rejects a null record pointer with `SCE_KERNEL_ERROR_EINVAL` (PPSA31544's `fakelib/libSceAgcDriver.sprx` passes the `Sce.Debug:Gnm` and `SceGnmDumpArea` keys). What the property controls and which fields the kernel validates are unmeasured

## add Functional

- [sceKernelGetToolMemoryRange, sceKernelAllocateToolMemory, sceKernelAvailableToolMemorySize, sceKernelMapToolMemory and sceKernelReleaseToolMemory](../../../core/libs/prx/libkernel/SysExports.cpp) (libkernel and libkernel_sys) - the tool-memory tier of the direct-memory allocator, from firmware 4.03's `libkernel.sprx`: the range query is dmem ioctl `0xC018801D`; allocate is `0xC0288001` with the selector where direct allocation writes zero; the rest carry the direct-memory parameter lists. The selector reaches the ioctl unvalidated, so any value routes to the one tracked pool and every selector reports `[0, sceKernelGetDirectMemorySize())`; the console kernel's partition set and per-partition ranges are unmeasured. Firmware's map also ORs `0x2400` and defaults a null base to `0x50000000000`, neither reproduced. `sceKernelQueryToolMemory` and `sceKernelGetDefaultToolMemorySize` stay unimplemented

# libkernel_sys

## remove `sceKernelGetToolMemoryRange`
