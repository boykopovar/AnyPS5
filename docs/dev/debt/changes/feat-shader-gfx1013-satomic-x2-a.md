# shader

## add Functional

- [GFX10.1 64-bit scalar atomics](../../../core/shader/recompiler/Translation/src/MemoryInstructions/ScalarBufferInstructions.cpp) (`s_atomic_*_x2`, opcodes 0xa0-0xa6) are not hardware-verified on gfx1013: the wave-uniform host execution count, unaligned/out-of-range fault ordering, GLC/DLC coherence and the SBASE-as-VCC form are unmeasured on the console. They read a 64-bit value from SBASE+SOFFSET+immediate, apply the operation through the physical-address atomic path, and return the old value to SDST when GLC is set; cmpswap uses SDST[1:0] as desired and SDST[3:2] as comparator. Source: pinned LLVM llvmorg-19.1.0 SMInstructions.td (`S_ATOMIC_*_X2`, `SM_Pseudo_Atomics`, `SMEM_Real_10Plus_common`): https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/SMInstructions.td. Closes when: measured on a PS5.
