# shader

## add Unknown function info

- [v_mul_lo_i32](../../../core/shader/recompiler/Translation/src/VectorInstructions.cpp) (VOP3 0x16b) (shader recompiler) - unverified on the PS5's gfx1013: the recompiler returns the low 32 bits of the product, as for `v_mul_lo_u32`, but gfx1010 (RX 5700 XT, AMD Windows driver 32.0.21045.5002) returns 0 for every input. Measured with the [hardware oracle](../HW_ORACLE.md) over 1,024 rows in wave32 and 512 in wave64 (boundary pairs and random), at IEEE 0 and 1, with VGPR, inline-constant and SGPR sources and as raw words: a destination preset to `0x5a5a5a5a` becomes 0, while `v_mul_lo_u32` and `v_mul_hi_i32` in the same kernel return the product. gfx1013 is gfx10.1 like gfx1010; RDNA2 (gfx103x) has no `v_mul_lo_i32`. [Vop3PackLdexp.cpp](../../../core/libs/prx/libSceAgcDriver/tests/execution/Vop3PackLdexp.cpp) expects the product. Closes when: measured on a PS5.
