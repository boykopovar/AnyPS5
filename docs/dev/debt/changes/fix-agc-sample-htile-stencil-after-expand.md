# libSceAgcDriver

## change [Native multisample stencil plane sampling]

- [Native multisample stencil plane sampling](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - the separate stencil plane of a native multisampled D32S8 target is sampled through an R8_UINT 2D MSAA descriptor at the plane with the target's extent, samples, tile mode and XOR and no metadata, as a stencil-aspect view of the host depth image or of its sample shadow, which then copies the stencil with the depth. With a depth-only HTILE this holds always; with the stencil kept in HTILE only after an in-place expand of depth and stencil, until the same events that end the depth expand. Another format, metadata or layout is refused, and a plane left unproven by a colour target refuses as listed above. Whether the console's reads of the plane through such a descriptor match the host stencil is not measured.
