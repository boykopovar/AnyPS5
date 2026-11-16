# libSceLibcInternal

## add Functional

- [_Atan, _FAtan, _LAtan, _Tan, _FTan and _LTan](../../../core/libs/prx/libSceLibcInternal/QuadrantTrig.cpp) (libSceLibcInternal) - not measured on the console; only the arguments Dinkumware's own callers pass are implemented, and the rest throw. `_Atan(x, flags)` takes x in [0, 1] and flags made of 0x2, 0x4 and 0x8, the values its `atan`, `atan2`, `asin` and `acos` pass, and returns atan(x), pi/2 - atan(x), pi/2 + atan(x) or pi - atan(x) for flags 0, 0x2, 0x4 and 0x6, negated with 0x8 ([callers](https://github.com/bergsteiger/realwork/blob/19b446ce8ad2adf82ab8ce7988bc003221accad2/common/components/rtl/external/Embarcadero/DelphiBerlin/cpprtl/Source/dinkumware64/source/xxatan2.h)). `_Tan(x, quadrants)` takes only the 0 that Dinkumware's `tan` passes and returns libc's `tan`. Results come from libc's msun `atan` and `tan`, not Dinkumware's polynomials, so last bits can differ. Closes when: measured on the module.
