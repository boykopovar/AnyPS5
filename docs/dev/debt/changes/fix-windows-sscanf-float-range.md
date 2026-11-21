# libc

## change [The scanf family](../../../core/libs/prx/libc/include/WindowsScanning.hpp)

- [The scanf family](../../../core/libs/prx/libc/include/WindowsScanning.hpp) (libc) returns 0 on Windows after input failure following only suppressed conversions (`sscanf("12", "%*d%d", &value)`). Linux ordinary narrow formats without `%c` use glibc, which returns `EOF` there. Ordinary Windows stream conversions still accept a `0x` prefix without a hexadecimal digit and consume its `x`; string scans use the shared prefix rejection rule. Incomplete floating exponents (`1e`) remain host-backed, where FreeBSD rolls back `e`. Linux host-backed `0x.`/`nan(` float conversions also follow glibc: 2.44 rejects both, while 2.39 yields 0/NaN and leaves `(`; console behavior for those inputs is unmeasured.

## change [floating scan errno]

- [Floating scan errno](../../../core/libs/prx/libc/src/Formatting.cpp) (libc): Linux plain narrow formats not routed through the shared scanner still inherit glibc's floating range errors. Windows narrow string scans and the wide scanner follow the `libc.prx` of PPSA07923 and PPSA05916: `errno` becomes `ERANGE` only when a decimal field overflows or underflows to zero as a `long double`, whatever the destination width; a hexadecimal field or a subnormal `long double` leaves it alone. Every NaN is stored as the default quiet NaN, and `%Lf` writes the 10 bytes of the value, leaving the rest of its 16 untouched. Floating-point exception flags and other SDK versions are unverified.
