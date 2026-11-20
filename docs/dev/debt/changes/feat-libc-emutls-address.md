# libc

## add Functional

- [__emutls_get_address](../../../core/libs/prx/libc/src/Emutls.cpp) (libc) - implements the 64-bit GCC/LLVM compiler-emulated TLS control layout, allocating an aligned copy of each variable's template per host thread and returning that thread's stable address. The control index is shared across threads and initialized once. The console firmware provider and behavior when guest fibers share a host thread have not been measured; Azahar PPSA99169's exact unresolved import is the motivation, but the title was not run with this change.
