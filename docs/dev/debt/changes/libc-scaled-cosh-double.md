# libc

## change [_Cosh]

- [_Cosh](../../../core/libs/prx/libc/src/Math.cpp) (libc) - console verification is limited to the [40 firmware 11.60 payload cases published by xXPBSXx](https://github.com/boykopovar/AnyPS5/pull/20370#issuecomment-6888258999), covering result bits, errno, invalid, overflow, divide-by-zero and inexact flags. Negative scales, signaling NaNs, subnormal operands and results, and nondefault rounding and flush modes remain unverified. The 18.738 exponential cutoff uses the existing `_Xbig` bytes from PPSA03671's bundled `libc.prx`; transition-neighbor results are unverified against the system module.
