# libc

## change [getsubopt]

- [getsubopt](../../../core/libs/prx/libc/src/GetSubOpt.cpp) (libc, NID `h630-qzWzew`) follows FreeBSD 11.4 [getsubopt.c](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/lib/libc/stdlib/getsubopt.c): suboptions delimited by commas or whitespace, token splitting on '=' with empty and leading '=' values preserved, and null parameter validation. A PS5 payload process (firmware 11.60) found it not exported by the console's `libSceLibcInternal.sprx` (`dlsym` returned null); a title's own `libc.prx` and the console behavior are unmeasured.
