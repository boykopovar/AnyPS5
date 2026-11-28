# shader

## add Functional

- [SDWA byte destinations on `v_frexp_mant_f16` and `v_frexp_exp_i16_f16`](../../../core/shader/recompiler/RdnaDecoder/src/RdnaVectorOpDecoder.cpp) - the PS5's gfx1013 is not measured. `v_frexp_mant_f16` and `v_frexp_exp_i16_f16` accept byte and word destination selectors (`BYTE_0`..`BYTE_3`, `WORD_0`, `WORD_1`) with `UNUSED_PAD`, `UNUSED_SEXT` and `UNUSED_PRESERVE` across `WORD_0`, `WORD_1` and `DWORD` f16 sources with `neg`/`abs`; `clamp`/`omod` are accepted on mant and thrown on exp. Sources: [RDNA2 ISA](https://gpuopen.com/download/rdna2-shader-instruction-set-architecture.pdf) §§6.8, 12.8 opcodes 89–90, 13.3.7 Table 88; LLVM 19.1.0 [VOP1Instructions.td](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/VOP1Instructions.td) and [SIPeepholeSDWA.cpp](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/SIPeepholeSDWA.cpp). Verified in Wave32/Wave64; console comparison remains open.
