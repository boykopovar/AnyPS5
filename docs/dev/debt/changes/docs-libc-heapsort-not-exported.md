# libc

## change [heapsort]

- [heapsort](../../../core/libs/prx/libc/src/Heapsort.cpp) (libc) - the console's `libSceLibcInternal.sprx` does not export `heapsort` (`dlsym` returned null in a PS5 payload process, firmware 11.60); a title's own `libc.prx` is unmeasured. A null base or comparator and a size product that overflows throw, where FreeBSD 11.4's [`heapsort.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/stdlib/heapsort.c) does not check them. The sort follows that file's `CREATE` and `SELECT` with its one-element scratch block (a failed `malloc` returns -1); its order of comparator calls is checked against a host build of that file, not on a console
