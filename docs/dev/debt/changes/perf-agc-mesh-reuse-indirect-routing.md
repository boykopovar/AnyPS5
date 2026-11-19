# libSceAgcDriver

## change [Distinct-vertex mesh grouping]

- [Distinct-vertex mesh grouping](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp) (`APS5_MESH_REUSE`) - opt-in. A plain triangle-list NGG draw groups its triangles by distinct vertices under `GE_CNTL`'s `VERT_GRP_SIZE` and `PRIM_GRP_SIZE` instead of by index position. The grouping and its group table are built on the CPU, for an indexed draw from its flushed index snapshot, and kept per snapshot and per index range and content; indirect draws resolve their arguments through the existing CPU path before building each group table, so GPU-written arguments can require a synchronization wait. Whether the console's geometry engine fills a subgroup with exactly these triangles is not measured; any grouping within the limits draws the same pixels.
