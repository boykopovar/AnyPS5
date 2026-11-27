# shader

## add Functional

- [Scalar f16 add, sub, subrev and mul NaN results](../../../core/shader/recompiler/Translation/src/FloatInstructions.cpp) - the PS5's gfx1013 is not measured. A NaN result is the first NaN of source 0 then source 1, after the negation of `v_sub_f16`'s source 1 or `v_subrev_f16`'s source 0, quieted only with IEEE_MODE=1, or 0xfe00 for an invalid operation, measured on RDNA2 (gfx1036) and pinned in `agc_driver_f16_scalar_arithmetic_nan`. On a device whose NaNs the startup probe accepts, the source-0-side NaN is passed as both native operands; otherwise the result is rebuilt from the sources.
