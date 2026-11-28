# libSceLibcInternal

## add Functional

- [`_Rint`, `_FRint` and `_LRint`](../../../core/libs/prx/libSceLibcInternal/Rint.cpp) (libSceLibcInternal, NIDs `TzxDRcns9e4`, `TLvAYmLtdVw`, `fJmOr59K8QY`) implement floating-point rounding to integral value according to IEEE-754 semantics using libc's `rint`, `rintf` and `rintl`. Prototypes follow Dinkumware V6.00 `xmath.h`. Console floating-point flags and rounding mode controls are unverified.

## change `_Restore_state` (NID `77uWF3Z2q90`), `_Rint`

- [Additional 3.20 imports](../../../core/libs/prx/libSceLibcInternal/InventoryExports2.cpp) (libSceLibcInternal) - `_Restore_state` (NID `77uWF3Z2q90`), `_Save_state` (NID `0ITXuJOqrSk`), `_Scanf` (NID `s+MeMHbB1Ro`); signatures and behavior are unverified and calls throw through `NotImplemented_nid_no_patch`.

## change `_FQuadph`, `_FRint`, `_FSincos`

- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_FQuadph`, `_FSincos`, `_FTan`

## change `_LQuadph`, `_LRint`, `_LSincos`

- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_LPlsw`, `_LPmsw`, `_LPow`, `_LQuad`, `_LQuadph`, `_LSincos`, `_LTan`
