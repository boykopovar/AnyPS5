# shader

## add Functional

- [SDWA byte destinations on `v_cvt_norm_i16_f16`](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - the PS5's gfx1013 is not measured. `v_cvt_norm_i16_f16` accepts byte and word destination selectors (`BYTE_0`..`BYTE_3`, `WORD_0`, `WORD_1`) with unused modes `UNUSED_PAD`, `UNUSED_SEXT` and `UNUSED_PRESERVE` with 16-bit float sources. Sources: [RDNA2 ISA](https://gpuopen.com/download/rdna2-shader-instruction-set-architecture.pdf) §§6.8, 12.8 opcode 99, 13.3.7 Table 88; LLVM 19.1.0 [VOP1Instructions.td](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/VOP1Instructions.td) and [SIPeepholeSDWA.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/SIPeepholeSDWA.cpp). Execution verified in Wave32 and Wave64; console comparison remains unmeasured.
