# shader

## change [DS src2 operations]

- [DS src2 operations](../../../core/shader/recompiler/Translation/src/MemoryInstructions/SharedMemoryInstructions.cpp) (`ds_*_src2_*`, GFX10.1 only) read `DS[B]` and update `DS[A]` with the matching atomic, so the read of `B` is not atomic with it. `B = A + 4 * sext(offset1[6:0]:offset0)` for every width. An `A` not aligned to the data size skips the operation, a `B` not aligned to it reads 0, and a `B` outside LDS reads 0, as [measured on gfx1010](https://github.com/boykopovar/AnyPS5/pull/19452#issuecomment-7108603690) (RX 5700 XT, single-lane wave32; not the console's gfx1013). The f32 forms gave the same result as `ds_add/min/max_f32` there. The `offset1[7]` form and GDS throw.
