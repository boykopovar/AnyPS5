# shader

## add Functional

- The [f64 output modifier overflow](../../../core/shader/recompiler/Translation/src/Float64Instructions.cpp) is not measured on the PS5's gfx1013, and on RDNA2 only for `v_add_f64`, `v_mul_f64`, `v_fma_f64`, `v_max_f64` and `v_floor_f64`; the other f64 instructions share the scaling. When `mul:2` or `mul:4` takes a finite f64 result past the largest finite value, the f16/f64 round field decides the result as it does for an overflow of the operation: infinity on the side the mode rounds toward, the largest finite value on the other (round 1 gives -max, round 2 +max, round 3 both). Measured with [tools/hw-oracle](../HW_ORACLE.md) on gfx1031 (RX 6750 XT) over 1,764 operand pairs with IEEE off and on, the four f16/f64 denormal modes and the four round modes, with no mismatches, and on gfx1036. The f32 scaling still gives infinity in the directed modes.
