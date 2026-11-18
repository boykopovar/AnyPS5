# shader

## change [SDWA ubyte-to-f32 destinations]

- [SDWA ubyte-to-f32 destinations](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) (shader recompiler) - the PS5's gfx1013 is not measured. `v_cvt_f32_ubyte0` to `v_cvt_f32_ubyte3` accept byte and word destinations with all source selects and unused modes (UNUSED_PAD, UNUSED_SEXT, UNUSED_PRESERVE). Byte N is taken from src0 after selection/extension, converted to f32, and the low result byte or word is placed with pad, sign-extend or preserve. Sources: [RDNA2 ISA](https://gpuopen.com/download/rdna2-shader-instruction-set-architecture.pdf) §12.8, §6.8, Table 88; LLVM 19.1.0 VOP1Instructions.td and SIPeepholeSDWA.cpp. Wave32/64 execution is verified in SdwaCvtF32Ubyte.cpp, and on RDNA2 (gfx1036, [hardware oracle](../HW_ORACLE.md)) every SDWA result is the plain result of the selected, extended source, placed; gfx1013 remains unmeasured.
