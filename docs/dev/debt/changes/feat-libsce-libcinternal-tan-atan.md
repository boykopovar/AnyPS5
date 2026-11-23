# libSceLibcInternal

## add Functional

- [_Atan, _FAtan, _LAtan, _Tan, _FTan and _LTan](../../../core/libs/prx/libSceLibcInternal/QuadrantTrig.cpp) (libSceLibcInternal) - not measured on the console. `_Atan` throws for x outside [0, 1] and for flags other than 0x2, 0x4 and 0x8, the values Dinkumware's `atan2`, `asin` and `acos` pass. `_Tan` follows Dinkumware V6.50's [xxxtan.h](https://github.com/bergsteiger/realwork/blob/19b446ce8ad2adf82ab8ce7988bc003221accad2/common/components/rtl/external/Embarcadero/DelphiBerlin/cpprtl/Source/dinkumware64/source/xxxtan.h) and the `retcode & 1` path of `xxxquad.h`: an odd quadrant argument takes x in units of pi. Results come from libc's msun `atan` and `tan`, not Dinkumware's polynomials, so last bits and `FE_INEXACT` can differ; the NaN is the x86 default NaN. Closes when: measured on the module.
