# libSceAgcDriver

## add Functional

- [S# without a mip filter](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) - which of the magnification and minification filters it samples with is not measured on a console or with the [hardware oracle](../HW_ORACLE.md). The host sampler takes MIN_LOD and MAX_LOD clamped to 1/256, so a LOD above 0 minifies at the base level and a MAX_LOD of 0 magnifies at every LOD. That follows Mesa 25.2.8, which programs OpenGL minification filters without mipmaps as MIP_FILTER none with the sampler's own LOD range ([si_state.c](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-25.2.8/src/gallium/drivers/radeonsi/si_state.c#L4312), [samplerobj.c](https://gitlab.freedesktop.org/mesa/mesa/-/blob/mesa-25.2.8/src/mesa/main/samplerobj.c#L130)). A MIN_LOD above MAX_LOD keeps a zero LOD range, which magnifies. Closes when: measured with the hardware oracle on RDNA2
