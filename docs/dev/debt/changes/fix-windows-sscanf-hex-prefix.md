# libc

## change [The scanf family](../../../core/libs/prx/libc/include/WindowsScanning.hpp)

- [The scanf family](../../../core/libs/prx/libc/include/WindowsScanning.hpp) (libc) returns 0 on Windows after input failure following only suppressed conversions (`sscanf("12", "%*d%d", &value)`). Linux ordinary narrow formats without `%c` use glibc, which returns `EOF` there. Ordinary Windows stream conversions still accept a `0x` prefix without a hexadecimal digit and consume its `x`. Incomplete floating exponents (`1e`) remain host-backed, where FreeBSD rolls back `e`. Linux host-backed `0x.`/`nan(` float conversions also follow glibc: 2.44 rejects both, while 2.39 yields 0/NaN and leaves `(`; console behavior for those inputs is unmeasured.
