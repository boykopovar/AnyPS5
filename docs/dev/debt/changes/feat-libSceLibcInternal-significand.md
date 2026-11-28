# libSceLibcInternal

## add Functional

- [significand and significandf](../../../core/libs/prx/libSceLibcInternal/Significand.cpp) (libSceLibcInternal, NIDs `Az3tTyAy380` and `L2YaHYQdmHw`) follow `libSceLibcInternal.sprx` firmware 13.60 at `0x59b40` and `0x59b70`: each calls its `ilogb` helper (`0x525f0` or `0x52670`), negates the 32-bit exponent including the `INT_MIN` wraparound, and passes the original value and negated exponent to `scalbn` (`0x59900` or `0x599e0`). The implementation reuses the existing libc `ilogb` and `scalbn` bodies, so their host floating-point exception and `errno` differences apply; those side effects were not compared with the firmware module. No console execution or concrete title import was measured.
