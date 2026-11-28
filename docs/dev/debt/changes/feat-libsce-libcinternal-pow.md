# libSceLibcInternal

## add Functional

- [_Pow, _FPow and _LPow](../../../core/libs/prx/libSceLibcInternal/ScaledPow.cpp) (libSceLibcInternal) - `_Pow` and `_FPow` ignore the `short*` exponent pointer and return `pow`/`powf`'s result, leaving the pointer unread and unmodified as measured from firmware 13.42 disassembly on #22452. `_LPow` delegates to `PowerScaled_nid_no_patch` backed by `PowerExtended` in `libc/src/MathExact.cpp`, which zeros the exponent when `y != 1`, rounds or truncates the exponent matching console firmware 13.40 bit-identically across 133 console cases and 1,000,000 cases, storing the low 16 bits of the integer shift in `*exponent` without post-scaling, and sets errno on overflow/underflow or domain error.
