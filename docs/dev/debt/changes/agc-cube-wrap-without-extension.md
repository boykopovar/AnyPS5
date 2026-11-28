# libSceAgcDriver

## change [Cube texture descriptors]

- [Cube texture descriptors](../../../core/libs/prx/libSceAgcDriver/Graphics/src/GuestTextureResource.cpp) of two to five slices are only sampled: the [materializer](../../../core/shader/recompiler/Optimization/src/ResourceMaterializer.cpp) rejects loads, stores and dimension queries through them, which on RDNA2 stop at `DEPTH` while the view holds six faces. Out-of-range cube-array sampling is unimplemented; PS5 equivalence is unmeasured. A range of fewer than six slices samples six faces from `BASE_ARRAY` (measured with `tools/hw-oracle` on gfx1036 for two to five slices, and on RX 6600, gfx1032, for equal `BASE_ARRAY` and `DEPTH`); one of six or more keeps every slice up to `DEPTH`.

## remove [Table S#]
