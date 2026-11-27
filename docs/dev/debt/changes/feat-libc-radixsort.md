# libc

## add Unknown function info

- [radixsort and sradixsort](../../../core/libs/prx/libc/src/RadixSort.cpp) (libc) - follow FreeBSD 11.4's [`radixsort.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/radixsort.c) at the pinned commit: the same algorithm and the same order among equal keys. `radixsort` is unstable, as in FreeBSD, and `sradixsort` keeps equal keys in their input order. `sradixsort` takes its temporary array from the guest heap and returns -1 with `ENOMEM` when it cannot allocate it. The strings and their pointer array are read and written in place, as FreeBSD does, so an invalid pointer faults. The console's behavior for the same input is not measured.
