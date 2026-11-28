# libkernel

## change [rfork_thread and sceKernelRdup]

- [rfork_thread](../../../core/libs/prx/libkernel/Unimplemented.cpp) (libkernel) throws. ProsperoWeb (PRWB00001) 0.2.1 imports it from `libkernel`, and calls FreeBSD's `pid_t rfork_thread(int flags, void *stack, int (*func)(void *), void *arg)` only from a start-up fork probe that is off by default ([`native_fork_probe.c`](https://github.com/kvaps/ProsperoWeb/blob/8cfe29c3cbd763c8c832f414f902b68b7d9a09bb/src/frontend/ps5/native/native_fork_probe.c#L1-L40)). It isn't reached in a run here, and a guest runs as the only process, with no other process to start. The same title's `sceKernelRdup` (`native_trace.c`) is implemented for its own pid and has its own entry.
