# shader

## add Functional

- [`v_cvt_i32_f64` and `v_cvt_u32_f64`](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) ignore VOP3 output scaling. The clamp bit controls the inexact exception in the RDNA ISA. The decoder records it, but the recompiler does not model the floating-point exception status. The integer results were compared across all eight encodings on RDNA2 (gfx1036). Exception status and physical PS5 behavior remain unmeasured.
