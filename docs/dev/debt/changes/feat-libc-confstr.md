# libc

## add Unknown function info

- [confstr](../../../core/libs/prx/libc/src/Confstr.cpp) (libc) - follows FreeBSD 11.4's [`confstr.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/confstr.c) with its `_CS_*` values from [`unistd.h`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/include/unistd.h) and `_PATH_STDPATH` from [`paths.h`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/include/paths.h). The guest is taken as LP64, so the environment checks match the PS5. The default PATH and the strings returned for each name are not measured on the console.
