# libc

## add Unknown function info

- [_Feraise](../../../core/libs/prx/libc/src/FloatingPoint.cpp) (libc) - rejects exception bits outside the six architectural flags (`0x3f`) with `std::invalid_argument` before modifying `errno` or x87/MXCSR state, following `feraiseexcept`'s host-safety guard rather than FreeBSD `feraiseexcept` or PS5 `libc.prx` (`& 0xffff`). Console behavior for unsupported bits is unverified.
