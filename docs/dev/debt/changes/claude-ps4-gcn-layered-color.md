# libSceAgcDriver

## change [PS4 color array slices]

- [PS4 color array slices](../../../core/libs/prx/libSceAgcDriver/Graphics/src/State.cpp) (libSceAgcDriver, PS4) - not checked on a console. A `CB_COLOR_VIEW` of one slice renders into that slice, at the surface address plus the slice index times the slice size. A view of several slices renders into its first slice when `PA_CL_VS_OUT_CNTL` bit 18 (USE_VTX_RENDER_TARGET_INDX) is clear; with it set, a single-sample view renders into the slice the vertex render target index selects, counted from the slice start, through the array's resident image (see Layered color targets), and a multisampled one throws, as does a slice max below the slice start. Bits [12:11] and [31:24] are ignored: shadPS4 defines no fields there. Later slices of `GcnDisplay2D` and `GcnThin2D` take the bank rotation the texture path gives that layer (see PS4 (GCN) array layer banks), and such a single-slice view of a multisampled target throws.
