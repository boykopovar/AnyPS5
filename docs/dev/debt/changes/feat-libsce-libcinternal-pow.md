# libSceLibcInternal

## add Functional

- [_Pow, _FPow and _LPow](../../../core/libs/prx/libSceLibcInternal/ScaledPow.cpp) (libSceLibcInternal) - `_Pow` and `_FPow` ignore the `short*` exponent pointer and return `pow`/`powf`'s result, leaving the pointer unread and unmodified as measured from firmware 13.42 disassembly on #22452. `_LPow` with a non-null exponent still throws `NotImplemented`. Results come from libc's msun functions, not Dinkumware's, so last bits can differ.
