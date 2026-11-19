# libSceAgcDriver

## change [Compressed FMASK reads]

- [Compressed FMASK reads](../../../core/libs/prx/libSceAgcDriver/Graphics/src/MultisampleTarget.cpp) - console equivalence is unmeasured. Reads without an exact resident native owner, other layouts, standalone compressed decoding, guest publication and recipe reuse throw. Owners require 2, 4 or 8 fragments, one per sample, one slice and mip, pipe-aligned DCC/CMASK, Z_64KB_X FMASK and unchanged, disjoint planes, each watched or compared after host-import settling. Snapshots use AMD PAL `c5e80007` `Gfx9Fmask::GetPackedExpandedValue`: `0x02`, `0xe4`, `0x76543210`; shader expansion gives `0x10`, `0x3210`, `0x76543210`, four bits per sample as Mesa `mesa-25.0.0` `nir_lower_ms_txf_to_fragment_fetch` reads it. Metadata stores invalidate the view, except a same-value CPU store to a compared plane.

# shader

## change [FMASK image loads]

- Direct [FMASK image loads](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) of 16 samples, CMASK compression (bar the 2-, 4- and 8-fragment native owner paths), packed accesses and other layouts throw. Uncompressed FMASK of up to 8 samples reads a single-mip 2D or 2D-array 8-, 16- or 32-bit `UINT` snapshot with `Z_64KB_X` addressing; selectors may name X, zero or one. `Fmask32_S8_F8` returns the stored word; others expand to 4 bits per sample, an index at or past the fragment count reading 8 (unknown), samples past the count 0. Layouts follow PAL's [expanded values](https://github.com/GPUOpen-Drivers/pal/blob/c5e800072a32f68b6ccc4422936d96167c6e0728/src/core/hw/gfxip/gfx9/gfx9MaskRam.cpp#L5005) and [FMASK surfaces](https://github.com/GPUOpen-Drivers/pal/blob/c5e800072a32f68b6ccc4422936d96167c6e0728/src/core/imported/addrlib/src/core/addrlib2.cpp#L877). Console results, unused nibbles included, are unmeasured; compressed color sampling is unsupported.
