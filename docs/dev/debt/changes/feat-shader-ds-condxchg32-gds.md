# shader

## change SharedMemoryInstructions.cpp) uses two 32-bit atomic conditional exchanges

- [`ds_condxchg32_rtn_b64`](../../../core/shader/recompiler/Translation/src/MemoryInstructions/SharedMemoryInstructions.cpp) uses two 32-bit atomic conditional exchanges. RDNA2 (gfx1035) measurements confirmed aligned return values, conditional bit clearing and 16-bit offsets; out-of-range accesses return zero without modifying LDS. Misaligned accesses fault the wave on gfx1035; the recompiler does not model this fault. Radeon 780M (gfx1103, RDNA3) measurements confirmed aligned return values, conditional bit clearing, EXEC masking and operand overlap in wave32 and wave64; contended cases passed per-dword conservation checks. Contended lane ordering and gfx1013 remain unmeasured; pair-wide indivisibility is not assumed, and concurrent 64-bit LDS atomics use a separate lock. VGPR pairs extending beyond v255 throw; allocated-VGPR EXEC nullification is not emulated.
