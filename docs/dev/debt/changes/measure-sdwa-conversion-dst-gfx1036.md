# shader

## change [SDWA f16-to-f32 destinations]

- [SDWA f16-to-f32 destinations](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - the PS5's gfx1013 is not measured. `v_cvt_f32_f16` accepts byte and word destinations with f16 source selects (WORD_0, WORD_1, DWORD) and destination unused modes (UNUSED_PAD, UNUSED_SEXT, UNUSED_PRESERVE). The 16-bit float is selected, converted to single-precision float, and the low result byte or word is placed as bits with pad, sign-extend or preserve. Sources: [RDNA2 ISA](https://gpuopen.com/download/rdna2-shader-instruction-set-architecture.pdf) §12.8 opcode 11, §6.8 and §13.3.7 Table 88; LLVM 19.1.0 VOP1Instructions.td and SIPeepholeSDWA.cpp. Wave32 and Wave64 execution is verified in SdwaDestination.cpp, and on RDNA2 (gfx1036, [hardware oracle](../HW_ORACLE.md)) every SDWA result is the plain result of the selected source, placed; gfx1013 is unmeasured.

## change [SDWA byte destinations on `v_cvt_f16_f32`]

- [SDWA byte destinations on `v_cvt_f16_f32`](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - the PS5's gfx1013 is not measured. `v_cvt_f16_f32` accepts byte and word destination selectors (`BYTE_0`..`BYTE_3`, `WORD_0`, `WORD_1`) with unused modes `UNUSED_PAD`, `UNUSED_SEXT` and `UNUSED_PRESERVE` across all source selectors (`BYTE_0`..`BYTE_3`, `WORD_0`, `WORD_1`, `DWORD`). Sources: [RDNA2 ISA](https://gpuopen.com/download/rdna2-shader-instruction-set-architecture.pdf) §§6.8, 12.8 opcode 10, 13.3.7 Table 88; LLVM 19.1.0 VOP1Instructions.td and SIPeepholeSDWA.cpp. Execution verified in Wave32 and Wave64, and on RDNA2 (gfx1036, [hardware oracle](../HW_ORACLE.md)) every SDWA result is the plain result of the selected, extended source, placed; gfx1013 is unmeasured.

## change [SDWA byte destinations on `v_cvt_i16_f16`]

- [SDWA byte destinations on `v_cvt_i16_f16`](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - the PS5's gfx1013 is not measured. `v_cvt_i16_f16` accepts byte and word destination selectors (`BYTE_0`..`BYTE_3`, `WORD_0`, `WORD_1`) with unused modes `UNUSED_PAD`, `UNUSED_SEXT` and `UNUSED_PRESERVE` with 16-bit float sources. Sources: [RDNA2 ISA](https://gpuopen.com/download/rdna2-shader-instruction-set-architecture.pdf) §§6.8, 12.8 opcode 83, 13.3.7 Table 88; LLVM 19.1.0 VOP1Instructions.td and SIPeepholeSDWA.cpp. Execution verified in Wave32 and Wave64, and on RDNA2 (gfx1036, [hardware oracle](../HW_ORACLE.md)) every SDWA result is the plain result of the selected source, placed; gfx1013 is unmeasured.
