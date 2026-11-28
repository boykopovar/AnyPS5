# shader

## add Functional

- [Constant texel offsets of image samples](../../../core/shader/recompiler/SpirvBackend/src/SpirvImageEmitter.cpp) are emitted as `ConstOffset` with the guest's full 6-bit range, -32 to 31, without checking the device's `minTexelOffset`/`maxTexelOffset`, which Vulkan requires. An NVIDIA GeForce RTX 5080 (driver 610.74) reports -8 to 7, and an `image_sample_lz_o` with an offset outside that range reads another texel: on a 16x16 32_FLOAT texture with clamp-to-last-texel addressing, offsets with a component outside that range, such as (31, 0), (-32, 0), (8, 0), (15, -16) and (5, 9), differed from RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)), offsets from -8 to 7 matched, and with wrap addressing every offset matched. gfx1036 reports -64 to 63 and matches everywhere, as do `image_gather4_o` offsets, whose range is -32 to 31 on both devices. The PS5's gfx1013 is not measured.
