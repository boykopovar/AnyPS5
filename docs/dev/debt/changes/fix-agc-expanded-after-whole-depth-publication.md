# libSceAgcDriver

## add Unknown function info

- [Native multisampled depth publication as an in-place expand](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver) - a native multisampled depth target with HTILE whose publication to guest memory committed every plane whole and unchanged by the guest is taken as expanded: the publication wrote its HTILE words expanded (ZMask 15, and SMem 3 with the stencil in HTILE) and its planes hold the host samples, so a descriptor without metadata is sampled from the host image as after an in-place expand draw, until the same HTILE clears, fills and clear draws end it. A later draw that recompresses tiles is not observed, as for the expand draw. Not measured on a console.
