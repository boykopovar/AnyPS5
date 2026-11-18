# libSceLibcInternal

## add Functional

- [_Pow, _FPow and _LPow](../../../core/libs/prx/libSceLibcInternal/ScaledPow.cpp) (libSceLibcInternal) - not measured on the console; only a null `short*` exponent, the argument Dinkumware's `pow` passes, is implemented, and returns libc's `pow`, `powf` or `powl`. A non-null exponent throws: what the module writes through it, and how it scales the result, is unknown. Results come from libc's msun functions, not Dinkumware's, so last bits can differ. The `(x, y, short* pex)` prototype and the `pow(x, y)` call `_Pow(x, y, 0)` are Dinkumware V6.50's ([xxpow.h](https://github.com/bergsteiger/realwork/blob/19b446ce8ad2adf82ab8ce7988bc003221accad2/common/components/rtl/external/Embarcadero/DelphiBerlin/cpprtl/Source/dinkumware64/source/xxpow.h)). Closes when: the module is measured with a non-null exponent.
