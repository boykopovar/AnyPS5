# libSceLibcInternal

## add Unknown function info

- [inet6_rthdr_* and inet6_rth_* routing-header functions](../../../core/libs/prx/libSceLibcInternal/Rthdr.cpp) (libSceLibcInternal) - follow FreeBSD 11.4's [`rthdr.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/net/rthdr.c) in its default build: type 0 only, the loose flag only, and `inet6_rthdr_getaddr` returning slot `idx` (index equal to the segment count is one past the last address). `inet6_rthdr_reverse` is not provided, as FreeBSD compiles it out. The console's layout and behavior are not measured.
