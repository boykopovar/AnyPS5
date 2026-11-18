# shader

## add Functional

- [SMEM S_ATOMIC integer forms](../../../core/shader/recompiler/Translation/src/MemoryInstructions/ScalarBufferInstructions.cpp) (shader recompiler) - unverified on the PS5's gfx1013. Encodings 0x87-0x8c (`s_atomic_umax`, `and`, `or`, `xor`, `inc`, `dec`) update a dword at sbase plus soffset plus the immediate offset and return the old value when GLC is set. Operations and operand roles follow [LLVM llvmorg-19.1.0 SMInstructions.td](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/SMInstructions.td) lines 1276-1340; ordering, faults and the non-returning form are unmeasured, lowered through the host device-scope physical-address atomic path. Closes when: measured on a PS5
