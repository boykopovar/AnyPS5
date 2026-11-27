# libc

## add Inventory exports declared elsewhere

- [bsearch](../../../core/libs/prx/libc/src/FileAndHeap.cpp) (libc) throws `std::invalid_argument` for a null `base` or comparator with a nonzero count, and `std::overflow_error` for a span past `SIZE_MAX`, where FreeBSD 11.4 [`bsearch.c`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libc/stdlib/bsearch.c) calls a null comparator, passes a null base to it, or wraps the address. Otherwise it probes as that file does: a zero element size probes only the base address, and a null key goes to the comparator. The console's `bsearch` is unmeasured.
