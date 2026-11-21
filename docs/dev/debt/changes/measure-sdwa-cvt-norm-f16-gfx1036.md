# shader

## change [SDWA f16 normal conversions]

- [SDWA f16 normal conversions](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - the PS5's gfx1013 is not measured. `v_cvt_norm_i16_f16` and `v_cvt_norm_u16_f16` accept word and DWORD source selectors with neg/abs, and word destinations with pad, sign-extend and preserve; extraction and modifiers precede the conversion and placement; unclamped and out-of-range selectors throw in decode. Sources: [RDNA2 ISA](https://gpuopen.com/download/rdna2-shader-instruction-set-architecture.pdf) §12.8 opcodes 0x63–0x64, §6.8; LLVM 19.1.0 VOP1Instructions.td SDWA rules. On RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) [SdwaCvtNormF16](../../../core/libs/prx/libSceAgcDriver/tests/execution/SdwaCvtNormF16.cpp)'s words match in wave32 and wave64, and both opcodes match its model on all 65,536 halves, rounding to nearest even and reading a flushed denormal as a signed zero.
