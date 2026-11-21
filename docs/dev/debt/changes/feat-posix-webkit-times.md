# libScePosixForWebKit

## add Functional

- [times](../../../core/libs/prx/libScePosixForWebKit/Times.cpp) (libScePosixForWebKit) - returns a FreeBSD 11.4-style 16-byte `tms` with signed 32-bit counters at 128 ticks per second. Host process user and system CPU use `GetProcessTimes` or `getrusage`; terminated-child counters remain zero, matching the current guest `getrusage(RUSAGE_CHILDREN)` behavior. The return value uses the guest monotonic nanosecond source at 128 Hz. Guest child accounting and the console provider for the raw Azahar import remain unverified.
