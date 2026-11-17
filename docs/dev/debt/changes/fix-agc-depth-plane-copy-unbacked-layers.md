# libSceAgcDriver

## add Functional

- [Depth plane copies of unbacked slices](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver): a device without sparse residency for the copy's format, without `residencyNonResidentStrict`, or with fewer array layers for sparse images than the T# declares still binds memory for every slice of a 2D-array depth plane copy, so a T# whose `LAST_ARRAY` runs far past the memory it covers allocates gigabytes (PPSA05684 samples its 2560x1440 depth plane with `LAST_ARRAY` up to 1792). Elsewhere the copy is a sparse image that binds memory only up to the last slice with a depth surface or committed guest memory; the unbound slices read zero, as uncommitted guest memory is read here. What the console returns for such slices is not measured.
