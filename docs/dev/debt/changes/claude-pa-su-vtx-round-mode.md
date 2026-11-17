# libSceAgcDriver

## add Functional

- [PA_SU_VTX_CNTL rounding](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver) - not measured on hardware: every `ROUND_MODE` ([gfx6.json](https://gitlab.freedesktop.org/mesa/mesa/-/blob/f97d7dcca0e51d7e20ed4342bb5ea8afea7db7fa/src/amd/registers/gfx6.json#L384-391)) is drawn with the host's snapping to `subPixelPrecisionBits`, whose rounding Vulkan leaves unspecified ([L1568-L1570](https://github.com/KhronosGroup/Vulkan-Docs/blob/193ebe195f47ffc5004683d1659e71c725658c4d/chapters/vertexpostproc.adoc#L1568-L1570)). A vertex can land one subpixel step from the console's, changing coverage only for samples that close to an edge; devices with fewer than 8 subpixel bits are not refused. `PIX_CENTER` 0, `QUANT_MODE` other than 5 (1/256 pixel) and reserved bits throw. CUSA00900 writes 0x29 (truncate).
