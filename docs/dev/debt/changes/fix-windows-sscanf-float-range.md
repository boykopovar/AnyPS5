# libc

## change leaves `errno`; on Linux `sscanf` and `vsscanf` follow the `sscanf_s` rule

- [Floating scan errno](../../../core/libs/prx/libc/src/Formatting.cpp) (libc): `sscanf` and `vsscanf` on Linux and Windows, and the wide scanner, follow the `libc.prx` of PPSA07923 and PPSA05916. `errno` becomes `ERANGE` only when a decimal field overflows or underflows to zero as a `long double`, whatever the destination width. Hexadecimal fields and subnormal `long double` results leave it unchanged. NaN is stored as the default quiet NaN, and `%Lf` writes the 10 bytes of the value, leaving the rest of its 16 untouched. Stream forms routed to the host scanner inherit host behavior. Floating-point exception flags and other SDK versions are unverified.

## remove leaves `errno`; on Linux it inherits glibc's.
