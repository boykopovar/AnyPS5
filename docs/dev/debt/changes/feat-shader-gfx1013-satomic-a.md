# shader

## add Unknown function info

- [Scalar atomics](../../../core/shader/recompiler/Translation/src/MemoryInstructions/ScalarBufferInstructions.cpp) (shader recompiler) - not measured on the PS5's gfx1013. The 32-bit `s_atomic_swap`, `s_atomic_cmpswap`, `s_atomic_add`, `s_atomic_sub`, `s_atomic_smin`, `s_atomic_umin` and `s_atomic_smax` forms on a raw SGPR address apply the matching device-scope atomic and, with `glc`, return the old value in the data SGPR. Semantics follow LLVM 19.1.0 `SMInstructions.td` lines 1276-1340 and `SIISelLowering.cpp` `LowerATOMIC_CMP_SWAP`, which packs cmpswap's data operand as `{new value, compare value}`. The access runs on one lane per host subgroup and is broadcast, so a wave64 guest split across two subgroups applies it twice. The pinned AMD RDNA ISA guides do not document SMEM atomics; operand order and `glc` return are unverified on gfx1013. `s_atomic_*_x2` and `s_buffer_atomic_*` throw. Closes when: measured on a PS5
