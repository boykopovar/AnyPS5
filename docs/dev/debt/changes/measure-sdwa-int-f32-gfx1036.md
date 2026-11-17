# shader

## change [SDWA integer-to-f32 destinations]

- [SDWA integer-to-f32 destinations](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - the PS5's gfx1013 is not measured. `v_cvt_f32_i32` and `v_cvt_f32_u32` accept byte/word destinations with all source selects and zero/sign extension. The integer is selected and extended, converted once, then the low result byte/word is placed as bits with pad, sign-extend or preserve. Sources: [RDNA2 ISA](https://gpuopen.com/download/rdna2-shader-instruction-set-architecture.pdf) §12.8 opcodes 5–6, §6.8 and §13.3.7 Table 88; LLVM 19.1.0 VOP1Instructions.td and SIPeepholeSDWA.cpp. Lavapipe readbacks cover exactly representable integers. On RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) an inexact integer is rounded in the `MODE` f32 round mode before placement: 193,536 words, every selector, 32 sources, four round modes, wave32 and wave64, match a correctly rounded model.
