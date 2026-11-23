# libSceAgcDriver

## add Unknown function info

- [Native multisample stencil plane read from memory](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - with the stencil kept in HTILE and no in-place expand, an R8_UINT 2D MSAA descriptor without metadata at the stencil plane (the target's extent, samples, Z_X tile mode and XOR) reads the plane's bytes in guest memory instead of the host stencil, while the host has not written the target since its last publication or import: the texture unit ignores HTILE, and those bytes are the ones published or imported, unchanged by an HTILE-only clear. After a host write the read still refuses, since the plane the console would read then depends on compression that is not modeled. The console's reads are not measured.
