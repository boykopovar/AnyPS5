# libSceLibcInternal

## add Functional

- [_Sincos, _FSincos and _LSincos](../../../core/libs/prx/libSceLibcInternal/QuadrantTrig.cpp) (libSceLibcInternal) - not measured on the console. The prototype `T _Sincos(T x, T* pcos)` and the special cases follow Dinkumware V6.50's [xxxsin.h](https://github.com/bergsteiger/realwork/blob/19b446ce8ad2adf82ab8ce7988bc003221accad2/common/components/rtl/external/Embarcadero/DelphiBerlin/cpprtl/Source/dinkumware64/source/xxxsin.h): NaN gives x for both, zero gives x and a cosine of 1, an infinity raises `_FE_INVALID` through `_Feraise` and gives NaN for both. Finite results come from libc's msun `sin` and `cos`, not Dinkumware's polynomials, so last bits and the `FE_INEXACT` flag can differ. A null or unwritable cosine pointer throws. Closes when: measured on the module.

## change `_Setgloballocale` (NID `bEk+WGOU90I`), `_Sincos`

- [Additional 3.20 imports](../../../core/libs/prx/libSceLibcInternal/InventoryExports2.cpp) (libSceLibcInternal) - `_Setgloballocale` (NID `bEk+WGOU90I`), `_Skip` (NID `Jp6dZm7545A`); signatures and behavior are unverified and calls throw through `NotImplemented_nid_no_patch`.
