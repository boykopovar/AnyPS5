# libSceLibcInternal

## add Unknown function info

- [inet_neta](../../../core/libs/prx/libSceLibcInternal/InetNeta.cpp) (libSceLibcInternal) - follows FreeBSD 11.4's [`inet_neta.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/inet/inet_neta.c) at the pinned commit: it writes each nonzero octet from the top down, separated by dots and skipping zero octets, and fails with `EMSGSIZE` when less than 5 bytes remain before an octet. The console's output for the same input is not measured.
