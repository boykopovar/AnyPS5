# libSceAgcDriver

## add Functional

- [Stencil replay depth feedback check](../../../core/libs/prx/libSceAgcDriver/Graphics/src/Draw.cpp) (libSceAgcDriver) - a replayed stencil export is refused as depth or stencil feedback when a buffer descriptor of the draw, a read-only buffer a cached build moves included, or a draw input read in place, overlaps the tiled footprint of a bound depth or stencil plane. Reads through buffer device addresses are not checked against the planes: the shared address space covers every registered heap, so it gives no range. Such a read of a bound plane sees the guest bytes as published before the draw, in every replay pass, as a draw that is not replayed sees them; console behaviour for a shader reading the depth it writes through memory is not measured. The colour target check still counts the whole shared address space of an address-based shader as a read.
