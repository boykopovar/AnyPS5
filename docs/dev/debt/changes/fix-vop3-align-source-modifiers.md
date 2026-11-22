# shader

## add Functional

- [VOP3 abs and neg on v_alignbit_b32 and v_alignbyte_b32](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) (shader recompiler) - the PS5's gfx1013 is not measured. On VOP3 `v_alignbit_b32` (0x14e) and `v_alignbyte_b32` (0x14f), `abs` clears and `neg` flips bit 31 of each source, both set it, before the `op_sel` byte select of src2 and the shift, so they never change the shift amount; `omod` and `clamp` leave the result unchanged. Measured on RDNA2 (gfx1036, AMD Windows driver 32.0.21045.29003) with the [hardware oracle](../HW_ORACLE.md) over every `abs`/`neg` combination, `omod` at IEEE 0 and 1 with f32 denormals flushed and kept, `clamp`, `op_sel`, and literal, inline-constant and SGPR sources: 314 encodings by 1024 lanes. Checked by [Vop3AlignSourceModifiers](../../../core/libs/prx/libSceAgcDriver/tests/execution/Vop3AlignSourceModifiers.cpp).
