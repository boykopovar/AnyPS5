# libkernel

## change [sceKernelGetResidentFmemCount](../../../core/libs/prx/libkernel/DirectMemory/Export.cpp) (libkernel)

- [sceKernelGetResidentFmemCount](../../../core/libs/prx/libkernel/DirectMemory/Export.cpp) (libkernel) validates null pointers (`0x8002000e`) with neither written, and answers any pid other than `0` or the caller's with `0x80020003`: a PS5 Pro payload process (13.60) returned `ESRCH` for `-1` as well, leaving both outputs untouched, so negative pids are not the special `EINVAL` case this entry previously guessed. For the current process (pid 0 or getpid()) it returns 0 and computes resident and mapped counts in 16 KiB pages from the flexible memory allocation state; on that payload process the two differed (209 and 1057), while here both counts are the same number, as no resident tracking is kept, and they include the loaded image segments. Arbitrary external process resident counts were not measured on console.
