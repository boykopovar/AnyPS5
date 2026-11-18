# shader

## add Functional

- [D16 image reads](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) (`image_load`, `image_sample*` and `image_gather4*` with D16) pack each component as RDNA2 does (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)): an f32 rounds toward zero, so a finite value beyond the half range becomes ±0x7bff and denormals truncate; a NaN keeps its sign and the top ten mantissa bits, with 1 when they are zero (0x7f800001 → 0x7c01, a 16_FLOAT signaling NaN unchanged); a 32-bit integer saturates to 0..0xffff or -32768..32767. A NaN that the host sampler canonicalizes before packing still differs; see the NaN texel entry. The PS5's gfx1013 is not measured.
