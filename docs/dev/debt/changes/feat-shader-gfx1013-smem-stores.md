# shader

## add Functional

- [gfx1013 scalar SMEM stores](../../../core/shader/recompiler/Translation/src/MemoryInstructions/ScalarBufferInstructions.cpp) (shader) - `s_store_dword`/`x2`/`x4` (GFX10.1 opcodes 0x10-0x12) decode and translate to per-dword byte-address stores from `SBASE + SOFFSET + offset`, with SDATA at word0[12:6]; nothing is measured on gfx1013, so address arithmetic, cache policy and faults are unverified. Operation and operand order come from LLVM llvmorg-19.1.0 `SMInstructions.td` (`SM_Store_Pseudo`, `SM_Real_Stores_gfx10`, lines 1223-1239). `s_scratch_store_*` (0x15-0x17) and `s_buffer_store_*` (0x18-0x1a) decode but throw: the wave-uniform/swizzled scratch layout is unsettled (as for `s_scratch_load`) and the SPIR-V backend has no writable scalar-buffer descriptor path. Closes when: measured on a PS5.
