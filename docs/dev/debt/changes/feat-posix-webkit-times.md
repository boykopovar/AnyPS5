# libScePosixForWebKit

## add Functional

- [times](../../../core/libs/prx/libScePosixForWebKit/Times.cpp) (libScePosixForWebKit) - returns a FreeBSD 11.4-style 16-byte `tms` with signed 32-bit counters at 128 ticks per second. Host process user and system CPU use `GetProcessTimes` or `getrusage`. The terminated-child counters in `times` remain zero. On Windows, the guest `getrusage(RUSAGE_CHILDREN)` reports CPU times for tracked direct children after they are reaped. The return value from `times` uses the guest monotonic nanosecond source at 128 Hz. Console child accounting and the console provider for the raw Azahar import remain unverified.
