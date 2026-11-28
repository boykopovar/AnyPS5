# libSceAgcDriver

## add Functional

- [Native multisampled depth publication after a clear](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) applies a verified HTILE or separate stencil clear before publishing the native image. The existing clear proof still requires complete tracked stores and rejects conflicting guest writes. Unrecorded and unproven clears still throw. A Vulkan regression reproduces the message reported for Formula Retro Racing: World Tour (PPSA17189, #15018). Title execution and console behavior remain unverified.
