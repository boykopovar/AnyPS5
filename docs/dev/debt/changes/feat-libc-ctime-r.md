# libc

## add Functional

- [ctime_r](../../../core/libs/prx/libc/src/TimeExtras.cpp) (libc) is not measured on a console. It formats `localtime_r` with `asctime_r`, so it has the `asctime_r` gaps listed under strftime and asctime in this file, returns null when `localtime_r` fails, and throws for a null buffer
