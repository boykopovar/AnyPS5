# libc

## add Unknown function info

- [valloc](../../../core/libs/prx/libc/src/Valloc.cpp) (libc) - follows FreeBSD 11.4's [`valloc.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/valloc.c): it allocates through `posix_memalign` with the alignment that `getpagesize` reports, and returns null on failure. The page size the PS5 reports is not measured, so the alignment is only as exact as that value.
