# shader

## change [DS src2 operations]

- [DS src2 operations](../../../core/shader/recompiler/Translation/src/MemoryInstructions/SharedMemoryInstructions.cpp) (`ds_*_src2_*`, GFX10.1 only) read `DS[B]` and update `DS[A]` with the matching atomic, so the read of `B` is not atomic with it. `B = A + 4 * sext(offset1[6:0]:offset0)`; the 64-bit forms take `A = ADDR & ~7` and round `B` down to 8 bytes, as [gem5](https://github.com/gem5/gem5/blob/f2372347661c5995db3896308fea4c946880b316/src/arch/amdgpu/vega/insts/ds.cc#L4178-L4183) and [CLRX](https://github.com/CLRX/CLRX-mirror/blob/4ddb41e2ba381db46c61c486a7da092af9d49313/doc/GcnInstrsDs.md#ds_add_src2_u64) describe them. The `offset1[7]` form and GDS throw; whether that form keeps `A[31:17]` is not documented. The f32 forms follow the LDS float atomics rule above, inferred from `ds_min/max/add_f32` measured on RDNA2 (gfx1035); no src2 form was measured on GFX10.1.
