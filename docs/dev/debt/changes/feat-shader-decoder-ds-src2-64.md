# shader

## change [DS src2 operations]

- [DS src2 operations](../../../core/shader/recompiler/Translation/src/MemoryInstructions/SharedMemoryInstructions.cpp) (`ds_*_src2_*`, GFX10.1 only) read `DS[B]`, with `B = A + 4 * sext(offset1[6:0]:offset0)`, and then update `DS[A]` with the matching 32-bit or 64-bit atomic, so the read of `B` is not atomic with the update of `A`. The form that takes the offset from the address (`offset1[7]` set) and GDS throw: whether the LDS address keeps `A[31:17]` in that form, and whether the 64-bit forms scale the offset by 4 or 8 on hardware, is not documented. `ds_min_src2_f32`, `ds_max_src2_f32` and `ds_add_src2_f32` follow the LDS float atomics rule above, inferred from `ds_min_f32`, `ds_max_f32` and `ds_add_f32` measured on RDNA2 (gfx1035), which has no src2 opcodes; the f32 and 64-bit src2 forms were not measured on GFX10.1.
