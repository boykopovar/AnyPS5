# shader

## add Functional

- The [`v_mullit_f32` output modifiers](../../../core/shader/recompiler/Translation/src/FloatInstructions.cpp) remain unmeasured on a PS5. On RDNA2 gfx1036 the lighting result scales only with IEEE off and f32 output denormals flushed. Clamp follows scaling and preserves a NaN payload with DX10_CLAMP off. Overflows from scaling a finite f32 by two or four follow the f32 rounding direction, including when the lighting operation produces its negative maximum-finite fallback value. The same overflow rule was measured for `v_add_f32`, `v_mul_f32` and `v_cubema_f32`.
