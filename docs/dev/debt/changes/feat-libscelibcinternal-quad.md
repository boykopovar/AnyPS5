# libSceLibcInternal

## add Functional

- [_Quad, _FQuad, _LQuad, _Quadph, _FQuadph and _LQuadph](../../../core/libs/prx/libSceLibcInternal/Quad.cpp) (libSceLibcInternal) - not measured on the console. Prototypes and steps follow Dinkumware V6.50's [xxxquad.h](https://github.com/bergsteiger/realwork/blob/19b446ce8ad2adf82ab8ce7988bc003221accad2/common/components/rtl/external/Embarcadero/DelphiBerlin/cpprtl/Source/dinkumware64/source/xxxquad.h) with `_FAST_QUAD` off; which variant the module uses is unknown. Below `huge_rad` the quadrant is Dinkumware's rounded `x * 2/pi`, above it the nearest one. The remainder comes from msun's `__kernel_rem_pio2` rounded once, not from Dinkumware's tables, so its last bit can differ; it is summed in long double, so `_LQuad` loses precision when the x87 precision control is below 64 bits. A NaN or infinite argument or phase throws, as no Dinkumware caller passes one, and so does a null pointer. Closes when: measured on the module.

## change `_Quad`, `_Quadph`

- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Plsw`, `_Pmsw`, `_Pow`, `_Printf`, `_Putfld`, `_Putstr`, `_Puttxt`, `_Readloc`
