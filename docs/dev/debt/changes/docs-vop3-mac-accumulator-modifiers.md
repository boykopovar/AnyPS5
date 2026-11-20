# shader

## add Functional

- [VOP3 accumulator modifiers of v_mac_f32 and v_mac_legacy_f32](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) (shader recompiler) - VOP3 `v_mac_f32` (0x11f) and `v_mac_legacy_f32` (0x106) throw `VOP3 source modifiers are not implemented` when the src2 `abs` or `neg` bit is set, which on `v_fmac_f32` and `v_fmac_f16` applies to the accumulator (#18285, #18286). Both are gfx10.1 encodings; the PS5's gfx1013 is not measured. On RDNA2 (gfx1036, [hardware oracle](../HW_ORACLE.md)), 0x106 is `v_fmac_legacy_f32`, and the bit acts as `v_fma_legacy_f32`'s src2 bit: each of the 48 `abs`/`neg` combinations with bit 2 set matched it on 704 of 704 rows (±0, ±1, denormals, ±infinity, NaNs) at IEEE 0 and 1. RDNA2's 0x106 is fused and gfx1010's `v_mac_legacy_f32` is not, so this is not evidence for the console.
