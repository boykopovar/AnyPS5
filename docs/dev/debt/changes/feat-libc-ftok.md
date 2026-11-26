# libc

## add Unknown function info

- [ftok](../../../core/libs/prx/libc/src/Ftok.cpp) (libc) - follows FreeBSD 11.4's [`ftok.c`](https://github.com/freebsd/freebsd-src/blob/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/gen/ftok.c): the key combines `id` (low 8 bits), the low 8 bits of `st_dev` and the low 16 bits of `st_ino` from `stat`, and returns -1 when `stat` fails. The keys depend on the device and inode numbers the PS5 reports, which are not measured, so a key may differ from the console's for the same file.
