# libkernel_sys

## change [pthread_getstack_np]

- [pthread_getstack_np](../../../core/libs/prx/libkernel_sys/ThreadQueries.cpp) (libkernel_sys) writes the calling thread's stack address and size, following firmware 13.42 `libkernel.sprx` (SHA-256 `4be40bf838a46e1ff83a1809960672526139b09dda6c4a6592cfeb485d29f243`) export `ruZ9hhQ8oUk` at `0xdb70`, read from disassembly. A payload process on firmware 11.60 (a title process may differ) measured that the module leaves the stack size in `rax` (0x200000 in its main thread, 262144 in a helper thread created with that size) and that it faults on a null address output without writing and on a null size output after the address was written; whether callers use that return value and a non-null unmapped output are unmeasured. Null or unwritable outputs return `EFAULT` here as a host-safety fallback, with the address written first as on the console. The main thread size here follows the host `RLIMIT_STACK` (8 MiB fallback), not that 0x200000.
