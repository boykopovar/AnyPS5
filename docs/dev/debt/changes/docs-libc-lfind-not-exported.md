# libc

## change [lfind and lsearch]

- [lfind and lsearch](../../../core/libs/prx/libc/src/FileAndHeap.cpp) (libc) follow POSIX.1-2008 and FreeBSD 11 [`lsearch.c`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libc/stdlib/lsearch.c): return the first match; `lsearch` appends and increments `*nelp` on a miss. A PS5 payload process (firmware 11.60) found neither exported by the console's `libSceLibcInternal.sprx` (`dlsym` returned null for both); a title's own `libc.prx` and the console behavior are unmeasured. A null `nelp` throws `std::invalid_argument`. A zero `size` scans nothing: `lfind` returns null and `lsearch` returns `base` and increments `*nelp`, wrapping, with any other arguments. Otherwise `lfind` returns null for `*nelp == 0`, and a null `base`, `key` or `compare` throws the same. A span past `SIZE_MAX` throws `std::overflow_error` before any comparator call, and an `lsearch` append past it after the scan, before copying or incrementing `*nelp`.
