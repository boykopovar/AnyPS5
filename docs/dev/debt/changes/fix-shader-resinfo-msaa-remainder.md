# shader

## change [`image_get_resinfo` on a 2D MSAA or 2D MSAA-array T#]

- [`image_get_resinfo` on a 2D MSAA or 2D MSAA-array T#](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) - a 2D MSAA T# with 2, 4, 8 or 16 fragments returns (width, height, 0, 1) under DIM 1D, 2D and 2D_MSAA, as measured in the entry below; a 16-fragment descriptor is accepted only for the query, and every other access to it keeps refusing it as an invalid descriptor. The query binds the T# as a multisampled sampled image, so a device that cannot sample a colour attachment at the T#'s sample count still throws there; [ImageResinfoDimension](../../../core/libs/prx/libSceAgcDriver/tests/execution/ImageResinfoDimension.cpp) checks it through Vulkan on RDNA3 (RX 7700 XT). A 2D MSAA-array T# still throws, since the driver binds no type-15 texture.
