# libc

## add Functional

- [wcsnlen](../../../core/libs/prx/libc/src/Strings.cpp) (libc) - console behavior is unverified. Follows FreeBSD 11.4 [`wcsnlen.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/string/wcsnlen.c) at the pinned commit: it returns the length of the string, at most `maxlen`. Wide characters are `char16_t`, as in `wcslen` and `wcsnlen_s` here. A null string throws `std::invalid_argument`, since FreeBSD does not define it.
