# libSceLibcInternal

## add Functional

- [`_Erfc`, `_FErfc` and `_LErfc`](../../../core/libs/prx/libSceLibcInternal/Export.cpp) use the one-argument prototypes and the positive-tail cutoff `Erf_small` from [Dinkumware's `xxxerfc.h`](https://github.com/bergsteiger/realwork/blob/19b446ce8ad2adf82ab8ce7988bc003221accad2/common/components/rtl/external/Embarcadero/DelphiBerlin/cpprtl/Source/dinkumware64/source/xxxerfc.h). Finite inputs at or above the type's representation of `1.523` use the corresponding guest `erfc` implementation. Inputs outside that range throw through `NotImplemented_nid_no_patch`. Native rounding, floating-point flags and console ABI remain unverified.

## change `_Deletegloballocale`, `_Dtento`, `_Erfc`, `_FAtan`

- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Deletegloballocale`, `_Dtento`, `_FAtan`

## change `_Deletegloballocale`, `_Erfc`, `_FAtan`

- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Deletegloballocale`, `_FAtan`

## change `_FErfc`, `_FHypot`, `_FLogpoly`

- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_FHypot`, `_FPlsw`, `_FPmsw`, `_FPow`, `_FQuad`

## change `_LAtan`, `_LErfc`, `_LHypot`, `_LLogpoly`

- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_LAtan`, `_LHypot`
