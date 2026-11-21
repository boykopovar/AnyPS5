# libSceLibcInternal

## add Functional

- [`_Tgamma`, `_FTgamma` and `_LTgamma`](../../../core/libs/prx/libSceLibcInternal/Tgamma.cpp) (libSceLibcInternal, NIDs `z-OrNOmb6kk`, `odPHnVL-rFg`, `jq4Srfnz9K8`) implement the true gamma function according to ISO C99 / IEEE-754 semantics using libc's `tgamma`, `tgammaf` and `tgammal`. Prototypes follow Dinkumware `xmath.h`. Console floating-point flags and rounding mode controls are unverified.

## remove `_Tgamma` (NID `z-OrNOmb6kk`)

## remove (libSceLibcInternal) - `_LTgamma`
