# libSceAgcDriver

## add Functional

- [1D reads of 1D arrays](../../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp) (libSceAgcDriver) - a sampled image whose shader shape is 1D and whose T# is a 1D array is built from its base slice alone, as the 2D reads of 2D arrays are: `FirstLayerDescriptor` narrows the T# and the texture's first-layer view is a 1D view of that slice. Measured on RDNA2 (gfx1036, [hardware oracle](../HW_ORACLE.md)), not the PS5's gfx1013: DIM 1D `image_sample_lz`, `image_sample`, `image_sample_l` and `image_load` through a 64-texel, six-slice 32_FLOAT 1D-array T# read slice `BASE_ARRAY` at `BASE_ARRAY` 0, 1 and 3 and ignore any further address VGPR, and the translation matches on 32 of 32 rows each on NVIDIA and AMD. Storage images read or written through a 1D shape are not narrowed and still throw on a 1D-array T#.
