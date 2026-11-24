# libc

## add Unknown function info

- [strsep](../../../core/libs/prx/libc/src/StringExtensions.cpp) (libc) - console execution and invalid-pointer behavior are unverified. Valid inputs follow FreeBSD 11.4 [strsep.c](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/string/strsep.c). Unreadable guest cursor, text or delimiter memory and unwritable required outputs throw `std::runtime_error`; read-only text is accepted when no delimiter needs replacing. Concurrent mapping or protection changes between validation and writes are not transactional.
