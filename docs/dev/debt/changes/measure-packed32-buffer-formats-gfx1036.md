# libSceAgcDriver

## change [Unsigned packed32 image views]

- [Unsigned packed32 image views](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) - codes 37, 41, 44 and 48 are not measured as images on AMD or PS5 hardware. RDNA2 ISA 8.2.8 Table 47 lists 44/48 for images and buffers but omits 37/41 (and existing 30/34). Layouts follow Mesa 418c4cfa6708a0e0b1175e72fb8fd27d3ca1615a, `src/amd/registers/gfx10-rsrc.json` and the name reversal in `src/amd/common/gfx10_format_table.py`: X/Y/Z widths 10/11/11 at offsets 0/10/21 for 37/41, X/Y/Z/W widths 2/10/10/10 at 0/2/12/22 for 44/48; RDNA2 (gfx1036, [hardware oracle](../HW_ORACLE.md)) typed buffer loads decode all four codes with these layouts. Sampled and storage views use R32_UINT with identity host swizzles; shader conversion applies guest swizzles. Converted DCC clear codes 0001/1110 remain refused. Render-target support is unchanged.
