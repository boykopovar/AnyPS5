# libkernel

## add Functional

- [sceKernelMemoryPoolBatch protect](../../../core/libs/prx/libkernel/DirectMemory/MemoryPoolExport.cpp) (libkernel) - a protect entry drops the protection bits outside `0x3F7`, as `sceKernelMprotect` does (see the `sceKernelMprotect protection bits` entry), which was not measured on a console: there the map calls and `sceKernelMtypeprotect` refuse such bits with `EINVAL`, so the console's pool batch may refuse them as well
