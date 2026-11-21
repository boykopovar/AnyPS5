# libc

## add Functional

- [swprintf and vswprintf with a size of 0](../../../core/libs/prx/libc/src/FormattingWide.cpp) (libc) write nothing, even through a null buffer, and return what a size of 1 returns, 0 for an empty result and -1 otherwise, with `errno` set only as a size of 1 sets it, as the `libc.prx` bundled with PPSA10122 01.101.000 (SDK 5.00) does when run natively. A null buffer with a nonzero size still returns -1 with `EINVAL` here, where the module writes through it; that case was not run.
