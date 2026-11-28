# libkernel

## add Functional

- [sceKernelBatchMap2 type-protect](../../../core/libs/prx/libkernel/DirectMemory/Export.cpp) (libkernel) - a type-protect entry whose protection has bits outside `0x3F7` returns `SCE_KERNEL_ERROR_EINVAL` (`0x80020016`) before its range is examined, so a null or unmapped start answers the same way, and the range keeps its protection and memory type; it used to reach `DoMtypeprotect`, whose protection validation threw `std::invalid_argument` and ended the title. Not measured on a console: the single `sceKernelMtypeprotect` it mirrors answers differently between console runs (see the `sceKernelMprotect protection bits` entry), so a console may accept a bit. The protect entry of the same call passes its protection through unchanged, as `sceKernelMprotect` does.
