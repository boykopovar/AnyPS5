# libSceAgcDriver

## change [11_11_10 float CB resolve]

- [11_11_10 float CB resolve](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver) - 2, 4 and 8 samples of matching packed float targets are resolved by averaging in exact integer units and truncating the result per field. A NaN takes priority over infinity and is canonicalized. This follows [the gfx1036 hardware CB measurements](https://github.com/boykopovar/AnyPS5/issues/11362#issuecomment-6989669263) on Mesa 26.0.6 RADV. Console hardware, 16 samples, EQAA and resolves between component orders are unverified.
