# libc

## change strndup and strdup

- [strndup and strdup](../../../core/libs/prx/libc/src/AllocatingStrings.cpp) (libc) return null with `errno` set to `EINVAL` (22) when `source` is null (and `limit` is non-zero for `strndup`) or with `EFAULT` (14) when the source buffer cannot be read, preventing a host segfault on invalid guest input. POSIX.1-2008 and FreeBSD 11.4 libc leave `strndup(NULL)` and `strdup(NULL)` undefined (faulting). The `libc.prx` that titles ship has no null check either (read from disassembly of PPSA13934's copy, not measured on a console): `strdup` (0x6b440) calls `strlen` and `strndup` (0x6b650) calls `strnlen` on the source before allocating, so both fault on an unreadable source there.
