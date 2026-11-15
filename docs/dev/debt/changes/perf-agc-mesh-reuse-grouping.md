# libSceAgcDriver

## add Opt-in draw paths

- [Distinct-vertex mesh grouping](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp) (`APS5_MESH_REUSE`) - opt-in. A plain triangle-list NGG draw groups its triangles by distinct vertices under `GE_CNTL`'s `VERT_GRP_SIZE` and `PRIM_GRP_SIZE` instead of by index position. The grouping and its group table are built on the CPU when the draw is planned and kept per index range and index content; an indirect draw of such a pipeline throws, since its index count is only known on the GPU. Whether the console's geometry engine fills a subgroup with exactly these triangles is not measured; any grouping within the limits draws the same pixels.
