# libSceAgcDriver

## add Unknown function info

- [Native multisampled depth plane writes before a whole HTILE clear](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - when a fill of a sampled native depth target's whole HTILE is noted, pending storage results and queued GPU writes over its depth plane (and its stencil plane, with the stencil in HTILE) are landed first, and writes made up to then are taken as superseded by that clear once every HTILE word proves it; a write after the fill still refuses sampling. That the console's cleared tiles ignore the plane bytes follows the HTILE ZMask and SMem meanings and is not measured.
