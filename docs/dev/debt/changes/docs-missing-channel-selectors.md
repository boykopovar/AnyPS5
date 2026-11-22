# libSceAgcDriver

## add Functional

- [DST_SEL of a channel a three-channel, 4_4, BC4, BC5 or BC6H format lacks](../../../core/libs/prx/libSceAgcDriver/Graphics/src/TextureFormat.cpp) (libSceAgcDriver) reads the host view's 0 for Y or Z and 1 for W. RDNA2 (gfx1036, AMD Windows driver 32.0.21045.16010, [hardware oracle](../HW_ORACLE.md)) reads channel i mod n of an n-channel format in `image_load` and point `image_sample_lz`: X,Y,Z,W gives x,y,z,x for 11_11_10_FLOAT, 10_11_11_FLOAT, 9_9_9_5_FLOAT and BC6H, x,y,x,y for 4_4 and BC5, and x,x,x,x for BC4, and W,Z,Y,X on 11_11_10_FLOAT gives x,z,y,x where AnyPS5 gives 1,z,y,x on an NVIDIA GeForce RTX 5080 (610.74) and on gfx1036 through Vulkan. 5_6_5 already reads x for W, and BC1 matches. The PS5's gfx1013 is not measured.
