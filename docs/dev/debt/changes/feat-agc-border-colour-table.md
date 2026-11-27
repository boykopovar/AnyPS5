# libSceAgcDriver

## add Functional

- [Border colour table](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestSamplerResource.cpp) (libSceAgcDriver) - custom table colours are unmeasured on a PS5. PPSA21567 writes the table, and its observed table-type sampler clamps to the edge. An S# with `BORDER_COLOR_TYPE` 3 and a border clamp mode on any axis reads four 32-bit floats at `base + BORDER_COLOR_PTR * 16` when its sampler is built. An integer image throws. The base is `TA_BC_BASE_ADDR`/`_HI` (context 0x20/0x21) for draws and `TA_CS_BC_BASE_ADDR`/`_HI` (user configuration 0x380/0x381) for dispatches, as radeonsi programs them, and a pair never written throws. Without `VK_EXT_custom_border_color` and `customBorderColorWithoutFormat` the sampler throws. Draws and dispatches that read the table skip the resource cache, so a later write to the entry applies on the next one. Sampler tables (`SamplerTable` bindings) still refuse every border mode
