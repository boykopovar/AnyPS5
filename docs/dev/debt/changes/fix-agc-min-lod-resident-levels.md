# libSceAgcDriver

## add Functional

- [Sampled textures with a MIN_LOD clamp](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureTiling.cpp) (libSceAgcDriver) - the levels below a T#'s MIN_LOD are not read, tracked or uploaded when the view is single-layer 2D with BASE_LEVEL 0, no DCC and one sample, and every level from the clamp on ends before every level under it in guest memory, as in a GFX10 chain, which stores levels 1 and 0 last. This assumes no instruction reads a level below MIN_LOD: an `image_load_mip` with an explicit level under the clamp is not measured on a console and gets undefined texels here. Arrays, cubes, 3D, DCC, depth-compare and sRGB-expanded views still take the whole chain. Closes when: measured on a PS5. Labels: console.
