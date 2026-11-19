# shader

## change [DS src2 operations]

- [DS src2 operations](../../../core/shader/recompiler/Translation/src/MemoryInstructions/SharedMemoryInstructions.cpp) (`ds_*_src2_*`) read `DS[A + 4 * sext(offset1[6:0]:offset0)]` and then update `DS[A]`, so the read is not atomic with the update; the 64-bit forms follow the same rule, with the qword source at `A + 4 * offset` measured on gfx900 (RX Vega 56, wave64: result at `A`, source unchanged, carry out of the low dword, signed and unsigned 64-bit min, `ds_max_src2_f64`, `ds_xor_src2_b64`, `ds_write_src2_b64`). Unmeasured: gfx1013, RDNA2 (LLVM rejects the mnemonics for gfx103x and a hand-encoded 64-bit form timed out on gfx1036), negative offsets, and a second offset of 1, which aborted on gfx900 while the translator computes the unaligned load. The address-derived `offset1[7]` form and GDS still throw. `ds_*_src2_f32` follow the LDS float atomics rule above, inferred from RDNA2; they were not measured on GFX10.1.
