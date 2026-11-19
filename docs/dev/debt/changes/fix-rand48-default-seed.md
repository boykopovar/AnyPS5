# libc

## add Functional

- [drand48, lrand48 and mrand48 before a seed](../../../core/libs/prx/libc/src/Random.cpp) (libc) start from the state `srand48(0)` sets, `{0x330e, 0, 0}`, as the `libc.prx` bundled with PPSA10122 01.101.000 does when run natively on Windows (the first values are `drand48` 0x2bbb62dc / 2^32, then `lrand48` 0x5ffcc9c0 and `mrand48` 0x18abd015); FreeBSD starts from `{0x330e, 0xabcd, 0x1234}`. Not measured on a console or against another title.
