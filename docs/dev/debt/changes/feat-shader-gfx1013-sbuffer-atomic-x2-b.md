# shader

## add Functional

- [S_BUFFER_ATOMIC_{UMAX,AND,OR,XOR,INC,DEC}_X2](../../../core/shader/recompiler/Translation/src/MemoryInstructions/ScalarBufferInstructions.cpp) (shader recompiler) are unverified on gfx1013, including cache-policy behaviour, ordering against other waves and faults for out-of-range or misaligned qwords. The non-buffer `S_ATOMIC_*` forms, `S_BUFFER_ATOMIC_CMPSWAP_X2`, the float forms and a 64-bit data pair in an odd SGPR throw. They apply the 64-bit sdata pair to the qword at the descriptor base plus the signed immediate and soffset; `inc`/`dec` use the data pair as the wrap limit and the returning (`glc`) form writes the old qword to the same pair, from LLVM llvmorg-19.1.0 `SMInstructions.td` (https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/SMInstructions.td) and AMD RDNA2 ISA section 12.13. Closes when: measured on a PS5.
