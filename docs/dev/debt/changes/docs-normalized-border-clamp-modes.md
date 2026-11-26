# libSceAgcDriver

## add Functional

- [Normalized S# clamp modes 4, 5 and 7](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) map to `CLAMP_TO_BORDER` (4) and `MIRROR_CLAMP_TO_EDGE` (5, 7), which RDNA2 does not match (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md), 8x1 32_FLOAT, `image_sample_lz`, x from -2.3 to 2.6, three border colour types). With a point filter mode 4 reads the edge texel past the edges and mode 7 mirrors once on [-1, 0) and reads the border from x >= 1 and x < -1. With a bilinear filter the coordinate is clamped to the image's edges, so modes 4 and 5 (after mirroring) return half the edge texel and half the border there, and mode 7 blends the border into the last texel. AnyPS5 on an RTX 5080 (610.74) and on gfx1036 returns the border for mode 4 and the edge texel for 5 and 7 instead. Modes 0, 1, 2, 3 and 6 match. The PS5's gfx1013 is not measured.
