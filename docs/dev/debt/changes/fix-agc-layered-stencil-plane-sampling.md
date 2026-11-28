# libSceAgcDriver

## add [Layered stencil plane sampling]

- [Layered stencil plane sampling](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (AGC driver) - single-sample `R8_UINT` and `R8_UNORM` 2D-array and cube views over a resident depth surface's stencil plane (one mip level, `baseLevel == 0`, `lastLevel == 0`) are assembled through `DepthPlaneCopy`, copying each resident slice from `VK_IMAGE_ASPECT_STENCIL_BIT` and detiling non-resident slices from guest memory into a 4-byte-aligned staging stride. Multi-mip layered stencil views, other 1-byte formats, multisampled stencil arrays, and cropped GCN layered stencil views still throw. Not measured on console hardware.
