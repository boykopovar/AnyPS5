# libc

## add Functional

- [hcreate, hsearch and hdestroy](../../../core/libs/prx/libc/src/HashSearch.cpp) (libc) - console behavior is unverified. They follow FreeBSD 11.4's [`hcreate.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/stdlib/hcreate.c) and [`hsearch_r.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/stdlib/hsearch_r.c): `hcreate` ignores `nel` and returns 1, `hsearch` creates the one table of 16 slots on first use and doubles it before an insertion would fill half of it, and a `FIND` that misses sets `ESRCH`. The slots hold the caller's key and data pointers and move when the table grows. Keys are read through the guest string copy, up to 4096 bytes.
