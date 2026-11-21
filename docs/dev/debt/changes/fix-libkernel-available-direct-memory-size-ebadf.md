# libkernel

## remove [sceKernelAvailableDirectMemorySize](

## add Functional

- [sceKernelAvailableDirectMemorySize](../../../core/libs/prx/libkernel/DirectMemory/Export.cpp) (libkernel) - returns `EBADF` (`0x80020009`) until the process first touches direct memory; `sceKernelGetDirectMemorySize` and operations that allocate, release, query or map direct memory mark it initialized, while this availability query does not initialize it. After initialization it retains the existing free-range search and output behavior. The initial error and unchanged outputs are tested before the size query; allocation-first initialization is tested separately. The initial-state result follows a PS5 firmware 13.60 payload probe. Titles open the direct-memory device during start-up, so they are not expected to see the error.