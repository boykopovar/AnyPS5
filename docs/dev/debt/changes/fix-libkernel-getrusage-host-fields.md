# libkernel

## add Functional

- [getrusage](../../../core/libs/prx/libkernel/System/src/Process.cpp) (libkernel) - `RUSAGE_CHILDREN` on Linux and macOS is the host `getrusage(RUSAGE_CHILDREN)`, so it also counts host helpers the runtime reaps, such as the [URL opener](../../../core/libs/prx/libSceSystemService/HostUrlLauncherPosix.cpp); on Windows it holds only the user and system times of tracked direct children. On Windows `ru_maxrss` is the peak working set from `GetProcessMemoryInfo`, for `RUSAGE_THREAD` too, as Linux reports it, where FreeBSD 11.4 keeps a per-thread maximum sampled in `statclock`; the fault, block I/O, signal and context-switch counters stay 0. Console child accounting is unverified.
