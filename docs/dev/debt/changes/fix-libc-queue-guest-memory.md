# libc

## change [insque and remque]

- [insque and remque](../../../core/libs/prx/libc/src/Queue.cpp) (libc) have not been verified on a console. A null element or an inaccessible required link throws. This checked-memory fallback is for host safety; console behavior with corrupted links is unverified. Required write ranges are checked before updating links. Linux x86-64 uses fault-recovered in-place copies to check writable ranges and reuses the open host-write guard for stores; other hosts and the Linux syscall fallback keep mapping probes. Concurrent mapping changes can still leave partial updates. Otherwise, the queue operations follow FreeBSD 11.4 [insque.c](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libc/stdlib/insque.c) and [remque.c](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libc/stdlib/remque.c).
