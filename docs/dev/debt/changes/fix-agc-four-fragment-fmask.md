# libSceAgcDriver

## change [Compressed FMASK reads]

- [Compressed FMASK reads](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) - console equivalence is unmeasured. Native owners require 2, 4 or 8 fragments, one per sample, one slice and mip, pipe-aligned DCC/CMASK, Z_64KB_X FMASK and unchanged, disjoint planes, each watched or compared after host-import settling. Other native layouts and guest publication throw; recipe reuse is disabled. Snapshots use AMD PAL `c5e80007` `Gfx9Fmask::GetPackedExpandedValue`: `0x02`, `0xe4`, `0x76543210`; shader expansion gives `0x10`, `0x3210`, `0x76543210`, four bits per sample as Mesa `mesa-25.0.0` `nir_lower_ms_txf_to_fragment_fetch` reads it. Metadata stores invalidate the view, except a same-value CPU store to a compared plane.

## add Functional

- [Standalone compressed FMASK](../../../core/libs/prx/libSceAgcDriver/Graphics/src/FmaskTexture.cpp) - console equivalence and cost on large surfaces are unmeasured. Only four samples with four fragments, one 2D slice and mip, Z_64KB_X and pipe-aligned CMASK are decoded. CMASK nibbles `0xc` and `0xe` select zero and stored packed mappings; other codes throw. The expanded code follows AMD PAL `c5e800072a32f68b6ccc4422936d96167c6e0728`, `gfx9MaskRam.cpp`, `Gfx9Cmask::GetInitialValue`. Addressing adapts the decoder from #4790 and Amoenus/AnyPS5 `822db494615d39f0ae7e6fb4d421fa2750645d77`. Both planes are settled and watched or compared before snapshot reuse; recipe reuse is disabled. Arrays, EQAA, other sample counts and metadata layouts throw. No console hardware oracle was run.

# shader

## change [FMASK image loads]

- Direct [FMASK image loads](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) of 16 samples, CMASK compression (bar the 2-, 4- and 8-fragment native owner paths), packed accesses and other layouts throw. Uncompressed FMASK of up to 8 samples reads a single-mip 2D or 2D-array 8-, 16- or 32-bit `UINT` snapshot with `Z_64KB_X` addressing; selectors may name X, zero or one. `Fmask32_S8_F8` returns the stored word; others expand to 4 bits per sample, an index at or past the fragment count reading 8 (unknown), samples past the count 0. Layouts follow PAL's [expanded values](https://github.com/GPUOpen-Drivers/pal/blob/c5e800072a32f68b6ccc4422936d96167c6e0728/src/core/hw/gfxip/gfx9/gfx9MaskRam.cpp#L5005) and [FMASK surfaces](https://github.com/GPUOpen-Drivers/pal/blob/c5e800072a32f68b6ccc4422936d96167c6e0728/src/core/imported/addrlib/src/core/addrlib2.cpp#L877). Console results, unused nibbles included, are unmeasured; compressed color sampling is unsupported.
