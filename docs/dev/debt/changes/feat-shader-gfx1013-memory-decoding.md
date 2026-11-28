# shader

## add Functional

- [gfx1013 memory forms](../../../core/shader/recompiler/RdnaDecoder/src/RdnaMemoryOpDecoder.cpp) - nine scalar stores, 52 scalar atomics and 15 64-bit DS SRC2 forms decode but have no IR translation and throw during translation. DS SRC2 with OFFSET1[7] set still throws during decoding. Console execution and encoding comparison are unverified. Encodings follow LLVM 19.1.0 [SMInstructions.td](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/SMInstructions.td#L1155-L1340) and [DSInstructions.td](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/DSInstructions.td#L1526-L1555). DS SRC2 retains a signed 15-bit displacement scaled by four; the public gfx1013 reference does not establish console execution semantics. Closes when: these forms execute correctly and encodings are compared on gfx1013 hardware.
