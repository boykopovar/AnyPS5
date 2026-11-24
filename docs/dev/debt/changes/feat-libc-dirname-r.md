# libc

## add Unknown function info

- [dirname_r](../../../core/libs/prx/libc/src/StringExtensions.cpp) (libc) - console export and error behavior are unverified; follows FreeBSD 11.4 [`dirname.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/dirname.c) reentrant `dirname_r`, writing the directory path to caller-supplied storage through `GuestCopyOut_nid_no_patch`. Trailing slashes are stripped and consecutive slashes collapsed. Paths without slashes or empty/null paths yield `.`, root paths yield `/`, and paths whose directory component exceeds 1023 bytes return null with `errno` 63 (`ENAMETOOLONG`). An unwritable buffer returns null with `errno` 14 (`EFAULT`), and a null buffer throws `std::invalid_argument`.
