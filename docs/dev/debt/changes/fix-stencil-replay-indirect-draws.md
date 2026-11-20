# libSceAgcDriver

## add Functional

- [Indirect stencil reference export replay](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp) (libSceAgcDriver) - an indirect draw (DRAW_INDIRECT, DRAW_INDEX_INDIRECT and their multi forms) that replays stencil reference exports always reads its records and record count on the CPU (`IndirectDrawPath::StencilReplay`), through the flush hook, which waits for the recorded GPU work that writes them, so it has the limits of the CPU record path listed under GPU-side indirect draw records and CPU reads behind an address-based build. The records are replayed in record order, each split into instances and triangles as a direct draw; with an export the prover cannot bound, that is 256 draws per triangle per instance. A record whose instance or vertex range passes 2^32 throws, as for direct draws. Expanding the records on the GPU, which would avoid the wait, is not implemented.
