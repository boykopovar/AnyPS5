# libSceAgcDriver

## add Functional

- [DST_SEL of a channel a three-channel or 4_4 format lacks](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) (libSceAgcDriver) reads the host view's 1 for W of 11_11_10_FLOAT, 10_11_11_FLOAT and 9_9_9_5_FLOAT, and 0 for Z and 1 for W of 4_4. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) reads channel i mod n of an n-channel format in `image_load` and point `image_sample_lz`: X,Y,Z,W gives x,y,z,x for those three-channel formats and x,y,x,y for 4_4, and W,Z,Y,X on 11_11_10_FLOAT gives x,z,y,x where AnyPS5 gives 1,z,y,x on an NVIDIA GeForce RTX 5080 (610.74) and on gfx1036 through Vulkan. 5_6_5 already reads x for W. Block-compressed formats and the PS5's gfx1013 are not measured.
