# shader

## add Functional

- [64-bit scalar atomics](../../../core/shader/recompiler/Translation/src/MemoryInstructions/ScalarBufferInstructions.cpp) (`s_atomic_umax_x2`, `_and_x2`, `_or_x2`, `_xor_x2`, `_inc_x2`, `_dec_x2`; GFX10.1 SMEM 0xa7-0xac) - gfx1013 execution and fault behaviour are unmeasured, and no RDNA hardware measurement backs the OPS lowering. Decoded and translated as 64-bit physical-address atomics (unsigned max, bitwise and/or/xor, wrap inc/dec) that return the old qword with `glc`. Operations, `SReg_64` base/data operands and old-value return follow LLVM llvmorg-19.1.0 `SMInstructions.td` lines 1276-1340 (https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/SMInstructions.td). `S_BUFFER_ATOMIC_*_X2`, the other `S_ATOMIC_*_X2` and the 32-bit `S_ATOMIC_*` forms still throw. Closes when: measured on a PS5.
