# libkernel_sys

## change [kenv]

- [kenv](../../../core/libs/prx/libkernel_sys/Export.cpp) (libkernel_sys) follows FreeBSD 11.4's [`sys_kenv`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_environment.c#L87-L204) for a caller in a jail with the default parameters: `KENV_SET` and `KENV_UNSET` fail with `EPERM` before the name is read, as [`prison_priv_check`](https://github.com/freebsd/freebsd-src/blob/release/11.4.0/sys/kern/kern_jail.c#L3060) denies `PRIV_KENV_SET` and `PRIV_KENV_UNSET` there. That a program runs jailed as uid 0 is the model agreed for the credential checks, not measured on a console. The environment stays empty here, so `KENV_GET` reads `ENOENT` and a dump returns 0, where a PS5 payload process (firmware 11.60) got 320 from `KENV_DUMP` with a null or 16-byte buffer and 0 with 8 KiB.
