# libSceAgcDriver

## add Unknown function info

- [HTILE of a mipmapped depth view](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver) - a depth target with mip levels and HTILE places each level's HTILE as AMD PAL's gfx10 addrlib `HwlComputeHtileInfo` does for SW_64KB_Z_X: one meta block for the packed mip tail first, when there is one, then the levels outside the tail from the smallest to mip 0, each its extent rounded up at that level (`GetMipSize`) in 1024x512 pixel blocks of 32 KiB, and a slice steps by the whole chain. A view of a level in the mip tail stays refused (its depth already is), and so does a view whose rounded-up extent needs more HTILE than its own extent, which the HTILE users size from. The offsets are not measured on the console, and the HTILE inside each level is taken to be laid out like a surface of that level's extent.
