# shader

## add Functional

- [Scalar store decoding](../../../core/shader/recompiler/RdnaDecoder/src/RdnaMemoryOpDecoder.cpp) (`s_store_dword`, `s_scratch_store_dword`, `s_buffer_store_dword` and their `x2`/`x4` forms) has no IR translation: executing any decoded store throws. Encodings, scalar data operands, signed 21-bit byte offsets and GLC/DLC follow [LLVM 19.1.0 SMInstructions.td](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/SMInstructions.td#L1154-L1231). Decoding is checked against LLVM's gfx1013 assembler, not console hardware. Closes when: all nine stores are translated and measured on gfx1013.
