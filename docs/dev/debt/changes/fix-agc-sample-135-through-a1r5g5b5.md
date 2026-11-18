# libSceAgcDriver

## change [Image format 135]

- [Image format 135](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) has no PS5 measurement. `Format1_5_5_5UNorm` uses X/Y/Z/W at bits 0/1/6/11 with widths 1/5/5/5, from Mesa commit `418c4cfa6708a0e0b1175e72fb8fd27d3ca1615a`, `src/amd/registers/gfx10-rsrc.json` and `src/amd/common/gfx10_format_table.py` (Mesa calls it `5_5_5_1`; the texture debt's `1_5_5_5` is code 134). Texels are rotated right by one bit into A1R5G5B5 (`RotateTextureWord`), which every Vulkan device samples (R5G5B5A1 is optional), and read with selectors A/B/G/R; storage uses R16_UINT views, and R16_UINT images where the host cannot sample and copy R5G5B5A1. Loads and filtering use the host's UNORM precision: lavapipe point samples and NVIDIA (RTX 3060, driver 610.88) loads quantize 5-bit channels to 8-bit UNORM, so tests allow 1/255. PS5 precision is unverified. FORCE_DEGAMMA remains refused for 135.
