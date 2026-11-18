# shader

## add Functional

- [Scalar buffer atomics](../../../core/shader/recompiler/Translation/src/MemoryInstructions/ScalarBufferInstructions.cpp) (shader recompiler) - The GFX10.1 `s_buffer_atomic_swap`, `_cmpswap`, `_add`, `_sub`, `_smin`, `_umin` and `_smax` SMEM forms are decoded and translated to the MUBUF buffer atomic IR operations: the descriptor comes from SBASE, the byte offset from the sign-extended 21-bit immediate plus SOFFSET, the data from SDST (the cmpswap comparator from the next SGPR), and the old dword is written back to SDST when GLC is set. The operations and operand order come from LLVM llvmorg-19.1.0 `SMInstructions.td` and `BUFInstructions.td`, not hardware. The console's gfx1013 is unmeasured: byte range, fault, coherency and ordering behaviour against the descriptor and concurrent vector accesses is unverified. Closes when: measured on a PS5
