# shader

## add Functional

- [D16 image reads](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (`image_load`, `image_sample*` and `image_gather4*` with D16) pack each component as RDNA2 does (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)). Texels of a 32-bit float format (22, 64, 74, 77) round toward zero, so a finite value beyond the half range becomes ±0x7bff and denormals truncate; 8- and 16-bit UNORM and SNORM texels round to nearest even, as measured over all 256 8-bit codes and 256 16-bit ones, and 16-bit float texels are exact. Through an image table or a registration module the format is not known and every float texel rounds to nearest. A NaN keeps its sign and top ten mantissa bits, with 1 when they are zero (0x7f800001 → 0x7c01), and a 32-bit integer saturates to 0..0xffff or -32768..32767. A NaN the host sampler canonicalizes first still differs; see the NaN texel entry. The PS5's gfx1013 is not measured.
