# shader

## change [`image_get_resinfo` on a 2D MSAA or 2D MSAA-array T#]

- [`image_get_resinfo` on a 2D MSAA or 2D MSAA-array T#](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) - a 2D MSAA T# with 2, 4 or 8 fragments returns (width, height, 0, 1) under DIM 1D and 2D_MSAA, and under DIM 2D when the program only queries the image, as measured in the entry below. [ImageResinfoDimension](../../../core/libs/prx/libSceAgcDriver/tests/execution/ImageResinfoDimension.cpp) checks it through Vulkan on RDNA3 (RX 7700 XT). A 2D MSAA-array T# still throws, since the driver binds no type-15 texture, and a 16-fragment T# is refused as an invalid descriptor.
