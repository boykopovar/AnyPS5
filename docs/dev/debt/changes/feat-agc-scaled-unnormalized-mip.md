# libSceAgcDriver

## add Functional

- [Unnormalized S# coordinates](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) - `FORCE_UNNORMALIZED` uses a Vulkan `unnormalizedCoordinates` sampler, so the S# LOD range, LOD bias and mip filter are not applied, which is exact only on a one-level view. On a view of more levels at mip 0 an S# used only by explicit-LOD `image_sample` forms, with a point mip filter, matching point or bilinear filters, no mirror and no LOD bias, instead decodes to a normalized sampler with edge or border addressing, and the shader divides the texel coordinates by the extent of the LOD it samples. The level is `trunc(clamp(lod, MIN_LOD, MAX_LOD) + 0.5)` (a chosen rule; Vulkan leaves the .5 tie open); counting its texels follows the title's shader (PPSA01317's `(a >> L) + 0.5`, LOD = L, #16650), not measured on RDNA or gfx1013 - nor are clamp mode 6's border, the tie and bilinear above level 0.

# shader

## change [Unnormalized S#s]

- [Unnormalized S#s](../../../core/shader/recompiler/Optimization/src/DescriptorBindingBuilder.cpp) are accepted only when direct and used only by `image_sample` without comparison or `_a`, on a single-layer 1D or 2D view that is one level at mip 0 (even when the image is also sampled through a normalized S#), or that only unnormalized S#s sample, each without a mip filter or only by `image_sample_lz` without minimum LOD or bias, or that is a 2D view of more levels at mip 0 read only by `image_sample` forms with an explicit LOD through unmirrored S#s with a point mip filter, matching point or bilinear minification and magnification, no reduction and no LOD bias, whose texel coordinates the shader divides by the selected level's extent. The level is `trunc(clamp(lod, MIN_LOD, MAX_LOD) + 0.5)`; counting its texels follows the title's shader (PPSA01317's `(a >> L) + 0.5`, LOD = L, #16650), not measured - nor RDNA/gfx1013, its .5 tie, clamp mode 6's border or bilinear above level 0.
