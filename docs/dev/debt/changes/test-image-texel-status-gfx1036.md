# shader

## change [MIMG `tfe` and `lwe`]

- [MIMG `tfe` and `lwe`](../../../core/shader/recompiler/RdnaDecoder/src/RdnaImageOpDecoder.cpp) never write the status VGPR after the data. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.29003, [hardware oracle](../HW_ORACLE.md)) leaves it unchanged too, in wave32 and wave64, for `tfe`, `lwe` and both on `image_load`, `image_load_mip`, `image_sample_lz`, `image_sample_l`, `image_gather4_lz`, `image_get_resinfo`, `image_get_lod` and `image_store`, also with `d16`, out-of-bounds texels, mips past the last level and a T# `MIN_LOD_WARN` above the LOD; [ImageTexelStatusBounds](../../../core/libs/prx/libSceAgcDriver/tests/execution/ImageTexelStatusBounds.cpp) pins that. Unmeasured: a fetch from an unmapped page of a partially resident texture (texture memory is always resident here) and the PS5's gfx1013. `lwe` on `image_get_lod` throws.
