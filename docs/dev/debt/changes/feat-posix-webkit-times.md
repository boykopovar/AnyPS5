# libScePosixForWebKit

## add Functional

- [times](../../../core/libs/prx/libScePosixForWebKit/Times.cpp) (libScePosixForWebKit) - the child counters differ from FreeBSD 11.4 `getrusage(RUSAGE_CHILDREN)`. On Windows they sum only direct children registered with `RegisterChildProcess` and reaped by the guest wait, and omit descendants a child reaped. On Linux and macOS they come from the host `getrusage(RUSAGE_CHILDREN)`, which also counts helper processes the host reaps (such as the URL launcher), while the guest `getrusage(RUSAGE_CHILDREN)` there reports zero. A null or unwritable output throws, where FreeBSD faults, and a failing host time query throws, where FreeBSD returns -1. Console child accounting and the console provider for the raw Azahar import are unverified. Otherwise the counters follow FreeBSD 11.4 libc's `times.c`, `CONVTCK` at 128 ticks per second.
