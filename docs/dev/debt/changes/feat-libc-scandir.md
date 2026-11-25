# libc

## add Functional

- [scandir and alphasort](../../../core/libs/prx/libc/src/Directory.cpp) (libc) - follow POSIX.1-2008 over the guest's `readdir` entries: each kept entry is copied to the guest heap, the returned array is sorted in place with this library's `qsort_r`, which passes the caller's comparison pointers into that array, as FreeBSD 11.4's [scandir.c](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/scandir.c) does, and failures leave the caller's list unset. A `readdir` error fails the call with its errno, where FreeBSD ends the list at it. The console's directory order and the guest `dirent` layout beyond the name are unverified.
