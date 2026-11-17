# libSceAgcDriver

## add Functional

- [Depth plane copies of unbacked slices](../../../core/libs/prx/libSceAgcDriver/Graphics/src/DepthSurface.cpp) (libSceAgcDriver): a size query on a 2D-array T# of a depth plane returns the slice count of its copy, not `LAST_ARRAY` + 1, when the slices past some point have neither a depth surface nor committed guest memory. The copy then ends one slice of zeros after the last slice that has either (and not before `BASE_ARRAY`); reads of later slices clamp to that slice, which reads as the uncommitted memory does. What the console returns for such reads is not measured. PPSA05684 samples its 2560x1440 depth plane with `LAST_ARRAY` up to 1792.
