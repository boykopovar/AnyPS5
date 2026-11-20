# libSceLibcInternal

## change `_Locterm`, `_Locvar`, `_Logpoly`

- [Additional imported names](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) - `_Ldtob`, `_Litob`, `_Lock_spin_lock`, `_Locsum`, `_Locterm`, `_Locvar`

## add Functional

- [_Logpoly, _FLogpoly and _LLogpoly](../../../core/libs/prx/libSceLibcInternal/Logpoly.cpp) (libSceLibcInternal) - follow firmware 13.40's `libSceLibcInternal.sprx`, read from its disassembly and measured on a PS5 in a payload process for 20 inputs per type in the four MXCSR rounding modes and, for `_LLogpoly`, the four x87 rounding modes at 24-bit, 53-bit and 64-bit precision. Other firmware and the floating-point exception flags are unmeasured.
