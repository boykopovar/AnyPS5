# libSceAgcDriver

## change [DISABLE_DEGAMMA]

- [DISABLE_DEGAMMA](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) - S# word 0 bit 31 (RDNA 2 SKIP_DEGAMMA) selects a compatible UNORM sampled view for a direct sRGB image, before filtering. Local gfx1032 hardware measurements cover R8, RG8, RGBA8, BC1/2/3/7, point and bilinear sampling, with two stable repeats: skipped sRGB equals UNORM in all64 lanes, and FORCE_DEGAMMA wins when both bits are set. Non-sRGB formats need no skip conversion. Bindless sRGB tables, textures without a compatible UNORM view and an sRGB image shared by samplers requesting different decode modes still throw. The shader path for devices without native R8/RG8 sRGB sampling is not extended. PS5 gfx1013 equivalence and Windows/macOS are unverified.
