# shader

## change fills each texel channel from the lowest data slot whose DST_SEL names it, but RDNA2

- [`StoreTexel`](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) still fills each texel channel of an `image_store` from the lowest data slot whose DST_SEL names it, and stores DST_SEL 0 and 1 as 0, for image table accesses (their module reads only descriptor word 3, not the format), D16 stores, USCALED and SSCALED formats, and packed or three-channel formats. Everywhere else it follows RDNA2's rule for the format's channel count. The rule is measured on RDNA2 (gfx1031) over all 1296 DST_SEL tuples and 15 DMASKs, with 32-bit data, for every non-packed UNORM, SNORM, UINT, SINT and FLOAT format with one, two or four 8-, 16- or 32-bit channels: no mismatch. D16 stores, USCALED and SSCALED formats and packed formats are not measured, and a 32_32_32_UINT store to a linear T# writes nothing there. The PS5's gfx1013 is not measured.

## remove also differs from RDNA2 for one- and four-channel formats
