# shader

## add Functional

- [S_BUFFER_ATOMIC dword operations](../../../core/shader/recompiler/Translation/src/MemoryInstructions/ScalarBufferInstructions.cpp) (shader recompiler) - gfx1013 is not measured. SMEM opcodes 0x47-0x4c (`S_BUFFER_ATOMIC_UMAX`, `_AND`, `_OR`, `_XOR`, `_INC`, `_DEC`) translate to the matching 32-bit `BufferAtomic*` IR ops on the four-SGPR V#; the byte address is the 21-bit offset plus `soffset`, and `sdst` holds both the data operand and, with `GLC`, the returned old value. Operand and return layout follow pinned [LLVM llvmorg-19.1.0 `SMInstructions.td`](https://github.com/llvm/llvm-project/blob/llvmorg-19.1.0/llvm/lib/Target/AMDGPU/SMInstructions.td) L1252-L1264, L1276-L1340. The descriptor is planned as a writable `Buffer` resource rather than the read-only `ScalarBuffer` path of `s_buffer_load`, so descriptor TYPE/bounds behavior and negative offsets are unverified. Closes when: measured on a PS5
